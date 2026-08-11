#include "cpu/cached_interp.h"
#include "cpu/cpu.h"
#include "cpu/idle_skip.h"
#include "cpu_instruction_impl.h"
#include "fpu_instruction_impl.h"
#include "cpu/jit/invalidate_hook.h"
#include "memory/bus.h"
#include "memory/memory_map.h"
#include "mmu/mmu.h"
#include "mmu/soft_tlb.h"
#include "mmu/tlb.h"
#include "n64_system/machine_advance.h"
#include "utils/log.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace N64 {
namespace Cpu {
namespace CachedInterp {

namespace {

constexpr uint32_t PAGE_SHIFT = 12;
constexpr uint32_t PAGE_SIZE = 1u << PAGE_SHIFT;
constexpr uint32_t WORDS_PER_PAGE = PAGE_SIZE / 4;
constexpr uint32_t RDRAM_PAGES = RDRAM_SIZE >> PAGE_SHIFT;
// Flush RSP + scheduler after this many guest cycles of soft-chained work.
// Far shorter than a half-line. Do not also flush on event deadlines: that
// hangs Kirby 64 ~6s in under --no-jit (idle-skip still hits wait-loop events).
constexpr int kAdvanceEveryCycles = 1024;

void op_cache_wrap(Cpu &cpu, instruction_t /*inst*/) {
    (void)cpu;
    CpuImpl::op_cache();
}

struct DecodePage {
    std::array<CachedWord, WORDS_PER_PAGE> entries{};
};

class DecodeCache {
  public:
    DecodeCache() { rdram_pages_.fill(nullptr); }

    void clear() {
        last_hit_page_ = nullptr;
        last_hit_paddr_ = 0xFFFFFFFFu;
        rdram_pages_.fill(nullptr);
        other_pages_.clear();
        page_storage_.clear();
    }

    void invalidate_page(uint32_t paddr) {
        const uint32_t page_idx = paddr >> PAGE_SHIFT;
        DecodePage *page = find_page(page_idx);
        if (!page)
            return;
        if (last_hit_page_ == page)
            last_hit_page_ = nullptr;
        page->entries.fill(CachedWord{});
    }

    void invalidate_range(uint32_t paddr, uint32_t length) {
        if (length == 0)
            return;
        const uint32_t start = paddr & ~(PAGE_SIZE - 1);
        const uint64_t end64 =
            static_cast<uint64_t>(paddr) + static_cast<uint64_t>(length) - 1;
        const uint32_t end = end64 > 0xffffffffu
                                 ? 0xffffffffu
                                 : static_cast<uint32_t>(end64);
        for (uint64_t p = start; p <= end; p += PAGE_SIZE)
            invalidate_page(static_cast<uint32_t>(p));
    }

    // Returns entry pointer; caller fills on miss.
    CachedWord *entry(uint32_t paddr) {
        const uint32_t page_idx = paddr >> PAGE_SHIFT;
        const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;
        DecodePage *page = get_or_create_page(page_idx);
        last_hit_page_ = page;
        last_hit_paddr_ = paddr;
        return &page->entries[word];
    }

    CachedWord *try_hit(uint32_t paddr) {
        if (last_hit_page_ && last_hit_paddr_ == paddr) {
            const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;
            CachedWord &e = last_hit_page_->entries[word];
            if (e.handler)
                return &e;
        }
        const uint32_t page_idx = paddr >> PAGE_SHIFT;
        const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;
        DecodePage *page = find_page(page_idx);
        if (!page)
            return nullptr;
        CachedWord &e = page->entries[word];
        if (!e.handler)
            return nullptr;
        last_hit_page_ = page;
        last_hit_paddr_ = paddr;
        return &e;
    }

  private:
    DecodePage *find_page(uint32_t page_idx) const {
        if (page_idx < RDRAM_PAGES)
            return rdram_pages_[page_idx];
        auto it = other_pages_.find(page_idx);
        if (it == other_pages_.end())
            return nullptr;
        return it->second.get();
    }

    DecodePage *get_or_create_page(uint32_t page_idx) {
        if (page_idx < RDRAM_PAGES) {
            DecodePage *&slot = rdram_pages_[page_idx];
            if (!slot) {
                auto page = std::make_unique<DecodePage>();
                slot = page.get();
                page_storage_.push_back(std::move(page));
            }
            return slot;
        }
        auto &page = other_pages_[page_idx];
        if (!page)
            page = std::make_unique<DecodePage>();
        return page.get();
    }

    std::array<DecodePage *, RDRAM_PAGES> rdram_pages_{};
    std::unordered_map<uint32_t, std::unique_ptr<DecodePage>> other_pages_;
    std::vector<std::unique_ptr<DecodePage>> page_storage_;
    DecodePage *last_hit_page_{nullptr};
    uint32_t last_hit_paddr_{0xFFFFFFFFu};
};

DecodeCache g_cache{};

void step_one_core(bool do_count) {
    auto &cpu = g_cpu();

    cpu.prev_delay_slot = cpu.delay_slot;
    cpu.delay_slot = false;

    if (cpu.should_service_interrupt()) {
        cpu.handle_exception(ExceptionCode::INTERRUPT, 0, false);
        // Fall through: fetch/execute the first handler instruction this step
        // (matches the historical interpreter).
    }

    const uint32_t pc32 = static_cast<uint32_t>(cpu.get_pc64());
    uint32_t paddr = 0;
    if (auto direct = Mmu::try_direct_map(pc32)) {
        paddr = direct.value();
    } else {
        std::optional<uint32_t> paddr_of_pc =
            Mmu::resolve_vaddr_cached(pc32, 4, Mmu::BusAccess::LOAD);
        if (!paddr_of_pc.has_value()) {
            cpu.handle_exception(
                g_tlb().get_tlb_exception_code(Mmu::BusAccess::LOAD), 0, false);
            return;
        }
        paddr = paddr_of_pc.value();
    }

    instruction_t inst{};
    Handler handler = nullptr;

    if (CachedWord *hit = g_cache.try_hit(paddr)) {
        inst.raw = hit->word;
        handler = hit->handler;
    } else {
        inst.raw = Memory::read_paddr32(paddr);
        handler = decode(inst);
        CachedWord *slot = g_cache.entry(paddr);
        slot->word = inst.raw;
        slot->handler = handler;
    }

    *cpu.prev_pc_ptr() = *cpu.pc_ptr();
    *cpu.pc_ptr() = *cpu.next_pc_ptr();
    *cpu.next_pc_ptr() += 4;

    handler(cpu, inst);
    if (do_count)
        cpu.add_count(CPU_CYCLES_PER_INST);
}

} // namespace

Handler decode(instruction_t inst) {
    uint8_t op = inst.op;
    switch (op) {
    case OPCODE_SPECIAL: {
        switch (inst.r_type.funct) {
        case SPECIAL_FUNCT_ADD:
            return &CpuImpl::op_add;
        case SPECIAL_FUNCT_ADDU:
            return &CpuImpl::op_addu;
        case SPECIAL_FUNCT_DADD:
            return &CpuImpl::op_dadd;
        case SPECIAL_FUNCT_DADDU:
            return &CpuImpl::op_daddu;
        case SPECIAL_FUNCT_SUB:
            return &CpuImpl::op_sub;
        case SPECIAL_FUNCT_SUBU:
            return &CpuImpl::op_subu;
        case SPECIAL_FUNCT_DSUB:
            return &CpuImpl::op_dsub;
        case SPECIAL_FUNCT_DSUBU:
            return &CpuImpl::op_dsubu;
        case SPECIAL_FUNCT_MULT:
            return &CpuImpl::op_mult;
        case SPECIAL_FUNCT_MULTU:
            return &CpuImpl::op_multu;
        case SPECIAL_FUNCT_DMULT:
            return &CpuImpl::op_dmult;
        case SPECIAL_FUNCT_DMULTU:
            return &CpuImpl::op_dmultu;
        case SPECIAL_FUNCT_DIV:
            return &CpuImpl::op_div;
        case SPECIAL_FUNCT_DIVU:
            return &CpuImpl::op_divu;
        case SPECIAL_FUNCT_DDIV:
            return &CpuImpl::op_ddiv;
        case SPECIAL_FUNCT_DDIVU:
            return &CpuImpl::op_ddivu;
        case SPECIAL_FUNCT_SLL:
            return &CpuImpl::op_sll;
        case SPECIAL_FUNCT_SRL:
            return &CpuImpl::op_srl;
        case SPECIAL_FUNCT_SRA:
            return &CpuImpl::op_sra;
        case SPECIAL_FUNCT_SRAV:
            return &CpuImpl::op_srav;
        case SPECIAL_FUNCT_SLLV:
            return &CpuImpl::op_sllv;
        case SPECIAL_FUNCT_SRLV:
            return &CpuImpl::op_srlv;
        case SPECIAL_FUNCT_SLT:
            return &CpuImpl::op_slt;
        case SPECIAL_FUNCT_SLTU:
            return &CpuImpl::op_sltu;
        case SPECIAL_FUNCT_AND:
            return &CpuImpl::op_and;
        case SPECIAL_FUNCT_OR:
            return &CpuImpl::op_or;
        case SPECIAL_FUNCT_XOR:
            return &CpuImpl::op_xor;
        case SPECIAL_FUNCT_NOR:
            return &CpuImpl::op_nor;
        case SPECIAL_FUNCT_JR:
            return &CpuImpl::op_jr;
        case SPECIAL_FUNCT_JALR:
            return &CpuImpl::op_jalr;
        case SPECIAL_FUNCT_SYSCALL:
            return &CpuImpl::op_syscall;
        case SPECIAL_FUNCT_BREAK:
            return &CpuImpl::op_break;
        case SPECIAL_FUNCT_MFHI:
            return &CpuImpl::op_mfhi;
        case SPECIAL_FUNCT_MFLO:
            return &CpuImpl::op_mflo;
        case SPECIAL_FUNCT_MTHI:
            return &CpuImpl::op_mthi;
        case SPECIAL_FUNCT_MTLO:
            return &CpuImpl::op_mtlo;
        case SPECIAL_FUNCT_TGE:
            return &CpuImpl::op_tge;
        case SPECIAL_FUNCT_TGEU:
            return &CpuImpl::op_tgeu;
        case SPECIAL_FUNCT_TLT:
            return &CpuImpl::op_tlt;
        case SPECIAL_FUNCT_TLTU:
            return &CpuImpl::op_tltu;
        case SPECIAL_FUNCT_TEQ:
            return &CpuImpl::op_teq;
        case SPECIAL_FUNCT_TNE:
            return &CpuImpl::op_tne;
        case SPECIAL_FUNCT_DSLL:
            return &CpuImpl::op_dsll;
        case SPECIAL_FUNCT_DSRL:
            return &CpuImpl::op_dsrl;
        case SPECIAL_FUNCT_DSRA:
            return &CpuImpl::op_dsra;
        case SPECIAL_FUNCT_DSLL32:
            return &CpuImpl::op_dsll32;
        case SPECIAL_FUNCT_DSRL32:
            return &CpuImpl::op_dsrl32;
        case SPECIAL_FUNCT_DSRA32:
            return &CpuImpl::op_dsra32;
        case SPECIAL_FUNCT_SYNC:
            return &CpuImpl::op_sync;
        default:
            Utils::abort("Unimplemented funct = {:#08b} for opcode = SPECIAL.",
                         static_cast<uint32_t>(inst.r_type.funct));
        }
    }
    case OPCODE_REGIMM: {
        switch (inst.i_type.rt) {
        case REGIMM_RT_BLTZ:
            return &CpuImpl::op_bltz;
        case REGIMM_RT_BLTZL:
            return &CpuImpl::op_bltzl;
        case REGIMM_RT_BGEZ:
            return &CpuImpl::op_bgez;
        case REGIMM_RT_BGEZL:
            return &CpuImpl::op_bgezl;
        case REGIMM_RT_BLTZAL:
            return &CpuImpl::op_bltzal;
        case REGIMM_RT_BGEZAL:
            return &CpuImpl::op_bgezal;
        default:
            Utils::abort("Unimplemented rt = {:#07b} for opcode = REGIMM.",
                         static_cast<uint32_t>(inst.i_type.rt));
        }
    }
    case OPCODE_J:
        return &CpuImpl::op_j;
    case OPCODE_JAL:
        return &CpuImpl::op_jal;
    case OPCODE_LB:
        return &CpuImpl::op_lb;
    case OPCODE_LBU:
        return &CpuImpl::op_lbu;
    case OPCODE_LH:
        return &CpuImpl::op_lh;
    case OPCODE_LHU:
        return &CpuImpl::op_lhu;
    case OPCODE_LW:
        return &CpuImpl::op_lw;
    case OPCODE_LWU:
        return &CpuImpl::op_lwu;
    case OPCODE_LWL:
        return &CpuImpl::op_lwl;
    case OPCODE_LWR:
        return &CpuImpl::op_lwr;
    case OPCODE_LUI:
        return &CpuImpl::op_lui;
    case OPCODE_LD:
        return &CpuImpl::op_ld;
    case OPCODE_LDL:
        return &CpuImpl::op_ldl;
    case OPCODE_LDR:
        return &CpuImpl::op_ldr;
    case OPCODE_LL:
        return &CpuImpl::op_ll;
    case OPCODE_LLD:
        return &CpuImpl::op_lld;
    case OPCODE_SB:
        return &CpuImpl::op_sb;
    case OPCODE_SH:
        return &CpuImpl::op_sh;
    case OPCODE_SWL:
        return &CpuImpl::op_swl;
    case OPCODE_SW:
        return &CpuImpl::op_sw;
    case OPCODE_SWR:
        return &CpuImpl::op_swr;
    case OPCODE_SD:
        return &CpuImpl::op_sd;
    case OPCODE_SDL:
        return &CpuImpl::op_sdl;
    case OPCODE_SDR:
        return &CpuImpl::op_sdr;
    case OPCODE_SC:
        return &CpuImpl::op_sc;
    case OPCODE_SCD:
        return &CpuImpl::op_scd;
    case OPCODE_ADDI:
        return &CpuImpl::op_addi;
    case OPCODE_ADDIU:
        return &CpuImpl::op_addiu;
    case OPCODE_DADDI:
        return &CpuImpl::op_daddi;
    case OPCODE_DADDIU:
        return &CpuImpl::op_daddiu;
    case OPCODE_ANDI:
        return &CpuImpl::op_andi;
    case OPCODE_ORI:
        return &CpuImpl::op_ori;
    case OPCODE_XORI:
        return &CpuImpl::op_xori;
    case OPCODE_BEQ:
        return &CpuImpl::op_beq;
    case OPCODE_BEQL:
        return &CpuImpl::op_beql;
    case OPCODE_BNE:
        return &CpuImpl::op_bne;
    case OPCODE_BNEL:
        return &CpuImpl::op_bnel;
    case OPCODE_BLEZ:
        return &CpuImpl::op_blez;
    case OPCODE_BLEZL:
        return &CpuImpl::op_blezl;
    case OPCODE_BGTZ:
        return &CpuImpl::op_bgtz;
    case OPCODE_BGTZL:
        return &CpuImpl::op_bgtzl;
    case OPCODE_CACHE:
        return &op_cache_wrap;
    case OPCODE_SLTI:
        return &CpuImpl::op_slti;
    case OPCODE_SLTIU:
        return &CpuImpl::op_sltiu;
    case OPCODE_LWC1:
        return &FpuImpl::op_lwc1;
    case OPCODE_LDC1:
        return &FpuImpl::op_ldc1;
    case OPCODE_SWC1:
        return &FpuImpl::op_swc1;
    case OPCODE_SDC1:
        return &FpuImpl::op_sdc1;
    case OPCODE_CP0: {
        if (inst.cop_r_like.last11 == 0) {
            switch (inst.cop_r_like.sub) {
            case COP_MFC:
                return &CpuImpl::op_mfc0;
            case COP_MTC:
                return &CpuImpl::op_mtc0;
            case COP_DMFC:
                return &CpuImpl::op_dmfc0;
            case COP_DMTC:
                return &CpuImpl::op_dmtc0;
            default:
                Utils::abort("Unimplemented CP0 inst. sub = {:#07b}",
                             static_cast<uint8_t>(inst.cop_r_like.sub));
            }
        } else {
            switch (inst.fr_type.funct) {
            case COP0_FUNCT_TLBWI:
                return &CpuImpl::op_tlbwi;
            case COP0_FUNCT_TLBWR:
                return &CpuImpl::op_tlbwr;
            case COP0_FUNCT_TLBP:
                return &CpuImpl::op_tlbp;
            case COP0_FUNCT_TLBR:
                return &CpuImpl::op_tlbr;
            case COP0_FUNCT_ERET:
                return &CpuImpl::op_eret;
            default:
                Utils::abort("Unimplemented CP0 inst. funct = {:#07b}",
                             static_cast<uint8_t>(inst.fr_type.funct));
            }
        }
    }
    case OPCODE_CP1: {
        switch (inst.r_type.rs) {
        case COP_MFC:
            return &FpuImpl::op_mfc1;
        case COP_DMFC:
            return &FpuImpl::op_dmfc1;
        case COP_CFC:
            return &FpuImpl::op_cfc1;
        case COP_MTC:
            return &FpuImpl::op_mtc1;
        case COP_DMTC:
            return &FpuImpl::op_dmtc1;
        case COP_CTC:
            return &FpuImpl::op_ctc1;
        case COP_BC:
            return &FpuImpl::op_bc1;
        case COP1_FMT_S:
        case COP1_FMT_D:
        case COP1_FMT_W:
        case COP1_FMT_L:
            return &FpuImpl::op_cop1_arith;
        default:
            Utils::abort("Unimplemented rs = {:#07b} for opcode = CP1.",
                         static_cast<uint32_t>(inst.r_type.rs));
        }
    }
    default:
        Utils::abort("Unimplemented opcode = {:#04x} ({:#08b})", op, op);
    }
}

void clear() { g_cache.clear(); }

void invalidate_page(uint32_t paddr) { g_cache.invalidate_page(paddr); }

void invalidate_range(uint32_t paddr, uint32_t length) {
    g_cache.invalidate_range(paddr, length);
}

void reset() {
    g_cache.clear();
    set_code_invalidate_hook([](uint32_t paddr, uint32_t length) {
        invalidate_range(paddr, length);
    });
}

void step_one() { step_one_core(/*do_count=*/true); }

int run(int budget) {
    if (budget < 1)
        budget = 1;

    int total = 0;
    int pending = 0;
    idle_skip_begin_slice(budget);

    // Soft-chain like JIT: COUNT advances per instruction; RSP + scheduler
    // are batched and flushed on the cycle budget.
    const auto flush_pending = [&]() {
        if (pending < 1)
            return;
        N64System::advance_after_cpu(pending);
        pending = 0;
    };

    while (total < budget) {
        // Always advance COUNT once per loop iteration so it stays aligned
        // with pending/scheduler even when step_one_core returns early (TLB).
        step_one_core(/*do_count=*/false);
        g_cpu().add_count(CPU_CYCLES_PER_INST);
        ++total;
        ++pending;
        idle_skip_consume(1);
        if (pending >= kAdvanceEveryCycles)
            flush_pending();
        if (idle_skip_pending()) {
            flush_pending();
            const int skipped = idle_skip_apply_pending();
            if (skipped > 0)
                total += skipped;
        }
    }
    if (idle_skip_pending()) {
        flush_pending();
        const int skipped = idle_skip_apply_pending();
        if (skipped > 0)
            total += skipped;
    }
    flush_pending();
    return total;
}

} // namespace CachedInterp
} // namespace Cpu
} // namespace N64
