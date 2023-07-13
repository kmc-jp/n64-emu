#include "cpu_operation.h"

#include "bus.h"
#include "mmu.h"

namespace N64 {
namespace Cpu {

class Cpu::Operation::Impl {
  public:
    static void branch_likely_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
        // 分岐成立時のみ遅延スロットを実行する
        cpu.delay_slot = true; // FIXME: correct?
        if (cond) {
            spdlog::debug("branch likely taken");
            cpu.next_pc = vaddr;
        } else {
            spdlog::debug("branch likely not taken");
            cpu.set_pc64(cpu.pc + 4);
        }
    }

    static void branch_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
        cpu.delay_slot = true;
        if (cond) {
            spdlog::debug("branch taken");
            cpu.next_pc = vaddr;
        } else {
            spdlog::debug("branch not taken");
        }
    }

    static void branch_likely_offset16(Cpu &cpu, bool cond,
                                       instruction_t inst) {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        offset <<= 2;
        spdlog::debug("pc <= pc {:+#x}?", (int64_t)offset);
        branch_likely_addr64(cpu, cond, cpu.pc + offset);
    }

    static void branch_offset16(Cpu &cpu, bool cond, instruction_t inst) {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        offset <<= 2;
        spdlog::debug("pc <= pc {:+#x}?", (int64_t)offset);
        branch_addr64(cpu, cond, cpu.pc + offset);
    }
};

void assert_encoding_is_valid(bool validity) {
    // should be able to ignore?
    assert(validity);
}

void Cpu::Operation::Execute(Cpu &cpu, instruction_t inst) {
    uint8_t op = inst.op;
    switch (op) {
    case OPCODE_SPECIAL: // various operations (R format)
    {
        switch (inst.r_type.funct) {
        case SPECIAL_FUNCT_ADD: // ADD
        {
            // TODO: throw exception
            // TODO: 32bit mode?
            // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L903
            assert_encoding_is_valid(inst.r_type.sa == 0);
            uint64_t rs = cpu.gpr.read(inst.r_type.rs);
            uint64_t rt = cpu.gpr.read(inst.r_type.rt);
            spdlog::debug("ADD: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, rs + rt);
        } break;
        case SPECIAL_FUNCT_SUB: // SUB
        {
            // TODO: throw exception
            // TODO: 32bit mode?
            assert_encoding_is_valid(inst.r_type.sa == 0);
            uint64_t rs = cpu.gpr.read(inst.r_type.rs);
            uint64_t rt = cpu.gpr.read(inst.r_type.rt);
            spdlog::debug("SUB: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, rs - rt);
        } break;
        case SPECIAL_FUNCT_ADDU: // ADDU
        {
            // TODO: 32bit mode?
            // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/mips_instructions.c#L915
            assert_encoding_is_valid(inst.r_type.sa == 0);
            uint64_t rs = cpu.gpr.read(inst.r_type.rs);
            uint64_t rt = cpu.gpr.read(inst.r_type.rt);
            spdlog::debug("ADDU: {} <= {} + {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, rs + rt);
        } break;
        case SPECIAL_FUNCT_SUBU: // SUBU
        {
            // TODO: 32bit mode?
            assert_encoding_is_valid(inst.r_type.sa == 0);
            uint64_t rs = cpu.gpr.read(inst.r_type.rs);
            uint64_t rt = cpu.gpr.read(inst.r_type.rt);
            spdlog::debug("SUBU: {} <= {} - {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, rs - rt);
        } break;
        case SPECIAL_FUNCT_SLL: // SLL
        {
            assert_encoding_is_valid(inst.r_type.rs == 0);
            uint64_t rt = cpu.gpr.read(inst.r_type.rt);
            uint8_t sa = inst.r_type.sa;
            spdlog::debug("SLL: {} <= {} << {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.sa]);
            cpu.gpr.write(inst.r_type.rd, rt << sa);
        } break;
        case SPECIAL_FUNCT_SLTU: // SLTU
        {
            assert_encoding_is_valid(inst.r_type.sa == 0);
            uint64_t rs = cpu.gpr.read(inst.r_type.rs); // unsigned
            uint64_t rt = cpu.gpr.read(inst.r_type.rt); // unsigned
            spdlog::debug("SLTU {} {} {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rt], GPR_NAMES[inst.r_type.sa]);
            if (rs < rt) {
                cpu.gpr.write(inst.r_type.rd, 1);
            } else {
                cpu.gpr.write(inst.r_type.rd, 0);
            }
        } break;
        case SPECIAL_FUNCT_AND: // ADD
        {
            assert_encoding_is_valid(inst.r_type.sa == 0);
            spdlog::debug("AND: {} <= {} & {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) &
                                              cpu.gpr.read(inst.r_type.rt));
        } break;
        case SPECIAL_FUNCT_OR: // OR
        {
            assert_encoding_is_valid(inst.r_type.sa == 0);
            spdlog::debug("OR: {} <= {} | {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) |
                                              cpu.gpr.read(inst.r_type.rt));
        } break;
        case SPECIAL_FUNCT_XOR: // XOR
        {
            assert_encoding_is_valid(inst.r_type.sa == 0);
            spdlog::debug("XOR: {} <= {} ^ {}", GPR_NAMES[inst.r_type.rd],
                          GPR_NAMES[inst.r_type.rs], GPR_NAMES[inst.r_type.rt]);
            cpu.gpr.write(inst.r_type.rd, cpu.gpr.read(inst.r_type.rs) ^
                                              cpu.gpr.read(inst.r_type.rt));
        } break;
        case SPECIAL_FUNCT_JR: // JR
        {
            assert_encoding_is_valid(inst.r_type.rt == 0 &&
                                     inst.r_type.rd == 0 &&
                                     inst.r_type.sa == 0);
            uint64_t rs = cpu.gpr.read(inst.r_type.rs);
            spdlog::debug("JR {}", GPR_NAMES[inst.r_type.rs]);
            Impl::branch_addr64(cpu, true, rs);
        } break;
        default: {
            spdlog::critical(
                "Unimplemented funct = {:#08b} for opcode = {:#08b}.",
                static_cast<uint32_t>(inst.r_type.funct), op);
            Utils::core_dump();
            exit(-1);
        } break;
        }
    } break;
    case OPCODE_LUI: // LUI (I format)
    {
        assert_encoding_is_valid(inst.i_type.rs == 0);
        int64_t simm = (int16_t)inst.i_type.imm; // sext
        simm <<= 16;
        spdlog::debug("LUI: {} <= {:#x}", GPR_NAMES[inst.i_type.rt], simm);
        cpu.gpr.write(inst.i_type.rt, simm);
    } break;
    case OPCODE_LW: // LW (I format)
    {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        spdlog::debug("LW: {} <= *({} + {:#x})", GPR_NAMES[inst.i_type.rt],
                      GPR_NAMES[inst.i_type.rs], offset);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        uint32_t paddr = Mmu::resolve_vaddr(vaddr);
        uint32_t word = Memory::read_paddr32(paddr);
        cpu.gpr.write(inst.i_type.rt, word);
    } break;
    case OPCODE_SW: // SW (I format)
    {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        spdlog::debug("SW: *({} + {:#x}) <= {}", GPR_NAMES[inst.i_type.rs],
                      offset, GPR_NAMES[inst.r_type.rt]);
        uint64_t vaddr = cpu.gpr.read(inst.i_type.rs) + offset;
        uint32_t paddr = Mmu::resolve_vaddr(vaddr);
        uint32_t word = cpu.gpr.read(inst.r_type.rt);
        Memory::write_paddr32(paddr, word);
    } break;
    case OPCODE_ADDIU: // ADDIU (I format)
    {
        int64_t imm = (int16_t)inst.i_type.imm; // sext
        int64_t tmp = cpu.gpr.read(inst.i_type.rs) + imm;
        spdlog::debug("ADDIU: {} <= {} + {:#x}", GPR_NAMES[inst.i_type.rt],
                      GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, tmp);
    } break;
    case OPCODE_ANDI: // ANDI (I format)
    {
        uint64_t imm = inst.i_type.imm; // zext
        spdlog::debug("ANDI: {} <= {} & {:#x}", GPR_NAMES[inst.i_type.rt],
                      GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) & imm);
    } break;
    case OPCODE_ORI: // ORI (I format)
    {
        uint64_t imm = inst.i_type.imm; // zext
        spdlog::debug("ORI: {} <= {} | {:#x}", GPR_NAMES[inst.i_type.rt],
                      GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) | imm);
    } break;
    case OPCODE_XORI: // XORI (I format)
    {
        uint64_t imm = inst.i_type.imm; // zext
        spdlog::debug("XORI: {} <= {} ^ {:#x}", GPR_NAMES[inst.i_type.rt],
                      GPR_NAMES[inst.i_type.rs], imm);
        cpu.gpr.write(inst.i_type.rt, cpu.gpr.read(inst.i_type.rs) ^ imm);
    } break;
    case OPCODE_BNE: // BNE (I format)
    {
        spdlog::debug("BNE: cond {} != {}", GPR_NAMES[inst.i_type.rs],
                      GPR_NAMES[inst.i_type.rt]);
        Impl::branch_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) != cpu.gpr.read(inst.i_type.rt),
            inst);
    } break;
    case OPCODE_BNEL: // BNEL (I format)
    {
        spdlog::debug("BNEL: cond {} != {}", GPR_NAMES[inst.i_type.rs],
                      GPR_NAMES[inst.i_type.rt]);
        Impl::branch_likely_offset16(
            cpu, cpu.gpr.read(inst.i_type.rs) != cpu.gpr.read(inst.i_type.rt),
            inst);
    } break;
    case OPCODE_CACHE: // CACHE
    {
        // B.1.1 CACHE Instruction
        // https://hack64.net/docs/VR43XX.pdf
        // no need for emulation?
        spdlog::debug("CACHE: no effect");

    } break;
    case OPCODE_COP0: // CP0 instructions
    {
        // https://hack64.net/docs/VR43XX.pdf p.86

        assert_encoding_is_valid(inst.copz_type1.should_be_zero == 0);
        switch (inst.copz_type1.sub) {
        case CP0_SUB_MF: // MFC0 (COPZ format)
        {
            spdlog::debug("MFC0: {} <= COP0.reg[{}]",
                          static_cast<uint32_t>(inst.copz_type1.rt),
                          GPR_NAMES[inst.copz_type1.rd]);
            const auto tmp =
                static_cast<uint32_t>(cpu.cop0.reg[inst.copz_type1.rd]);
            cpu.gpr.write(inst.copz_type1.rt, tmp);
        } break;
        case CP0_SUB_MT: // MTC0 (COPZ format)
        {
            spdlog::debug("MTC0: COP0.reg[{}] <= {}",
                          static_cast<uint32_t>(inst.copz_type1.rd),
                          GPR_NAMES[inst.copz_type1.rt]);
            const uint32_t tmp = cpu.gpr.read(inst.copz_type1.rt);
            cpu.cop0.reg[inst.copz_type1.rd] = tmp;
            // TODO: COP0を32bitレジスタに修正したあと、このあたりを見直す
        } break;
        case CP0_SUB_DMF: // DMFC0 (COPZ format)
        {
            spdlog::debug("DMFC0: {} <= COP0.reg[{}]",
                          static_cast<uint32_t>(inst.copz_type1.rt),
                          GPR_NAMES[inst.copz_type1.rd]);
            const uint64_t tmp = cpu.cop0.reg[inst.copz_type1.rd];
            cpu.gpr.write(inst.copz_type1.rt, tmp);
        } break;
        case CP0_SUB_DMT: // DMTC0 (COPZ format)
        {
            spdlog::debug("DMTC0: COP0.reg[{}] <= {}",
                          static_cast<uint32_t>(inst.copz_type1.rd),
                          GPR_NAMES[inst.copz_type1.rt]);
            const uint64_t tmp = cpu.gpr.read(inst.copz_type1.rt);
            cpu.cop0.reg[inst.copz_type1.rd] = tmp;
        } break;

        default: {
            spdlog::critical("Unimplemented CP0 inst. sub = {:07b}",
                             static_cast<uint8_t>(inst.copz_type1.sub));
            Utils::core_dump();
            exit(-1);
        }
        }
    } break;
    default: {
        spdlog::critical("Unimplemented opcode = {:#04x} ({:#08b})", op, op);
        Utils::core_dump();
        exit(-1);
    }
    }
}

} // namespace Cpu
} // namespace N64