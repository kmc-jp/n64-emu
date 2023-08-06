#ifndef CPU_H
#define CPU_H

#include "cop0.h"
#include "cop1.h"
#include "instruction.h"
#include <cstdint>

namespace N64 {
namespace Cpu {

const uint32_t CPU_CYCLES_PER_INST = 1;
constexpr uint8_t RA = 31;

enum class ExceptionCode : uint32_t {
    INTERRUPT = 0,
    TLB_MODIFICATION = 1,
    TLB_MISS_LOAD = 2,
    TLB_MISS_STORE = 3,
    ADDRESS_ERROR_LOAD = 4,
    ADDRESS_ERROR_STORE = 5,
    BUS_ERROR_INS_FETCH = 6,
    BUS_ERROR_LOAD_STORE = 7,
    SYSCALL = 8,
    BREAKPOINT = 9,
    RESERVED_INSTR = 10,
    COPROCESSOR_UNUSABLE = 11,
    ARITHMETIC_OVERFLOW = 12,
    TRAP = 13,
    FLOATING_POINT = 15,
    WATCH = 23,
};

class Gpr {
  private:
    std::array<uint64_t, 32> reg{};

  public:
    uint64_t read(uint32_t reg_num) const {
        assert(reg_num < 32);
        if (reg_num == 0) {
            return 0;
        } else {
            return reg[reg_num];
        }
    }

    void write(uint32_t reg_num, uint64_t value) {
        assert(reg_num < 32);
        if (reg_num != 0) {
            reg[reg_num] = value;
        }
    }
};

class Cpu {
  public:
    Gpr gpr;

    // special registers
    // 1.3
    // https://ultra64.ca/files/documentation/silicon-graphics/SGI_R4300_RISC_Processor_Specification_REV2.2.pdf
    uint64_t lo;
    uint64_t hi;

    // branch delay slot?
    bool delay_slot;
    bool prev_delay_slot;

    Cop0 cop0;
    Cop1 cop1;

    Cpu() {}

    void reset();

    void dump();

    void set_pc64(uint64_t value);

    uint64_t get_pc64() const;

    // CPUの1ステップを実行する
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/r4300i.c#L758
    void step();

    void execute_instruction(instruction_t inst);

    inline static Cpu &get_instance() { return instance; }

    static void branch_likely_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
        // 分岐成立時のみ遅延スロットを実行する
        cpu.delay_slot = true; // FIXME: correct?
        if (cond) {
            Utils::trace("branch likely taken");
            cpu.next_pc = vaddr;
        } else {
            Utils::trace("branch likely not taken");
            cpu.set_pc64(cpu.pc + 4);
        }
    }

    static void branch_addr64(Cpu &cpu, bool cond, uint64_t vaddr) {
        cpu.delay_slot = true;
        if (cond) {
            Utils::trace("branch taken");
            cpu.next_pc = vaddr;
        } else {
            Utils::trace("branch not taken");
        }
    }

    static void branch_likely_offset16(Cpu &cpu, bool cond,
                                       instruction_t inst) {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        // 負数の左シフトはUBなので乗算で実装
        offset *= 4;
        Utils::trace("pc <= pc {:+#x}?", (int64_t)offset);
        branch_likely_addr64(cpu, cond, cpu.pc + offset);
    }

    static void branch_offset16(Cpu &cpu, bool cond, instruction_t inst) {
        int64_t offset = (int16_t)inst.i_type.imm; // sext
        // 負数の左シフトはUBなので乗算で実装
        offset *= 4;
        Utils::trace("pc <= pc {:+#x}?", (int64_t)offset);
        branch_addr64(cpu, cond, cpu.pc + offset);
    }

    static void link(Cpu &cpu, uint8_t reg) { cpu.gpr.write(reg, cpu.pc + 4); }

    // TODO: move to somewhere else
    enum class BusAccess {
        LOAD,
        STORE,
    };

    // TODO: move to somewhere else
    static inline ExceptionCode get_tlb_exception_code(BusAccess bus_access) {
        Utils::unimplemented("get_tlb_exception_code");
        exit(-1);
        return ExceptionCode::INTERRUPT;
    }

    void handle_exception(ExceptionCode exception_code,
                          uint8_t coprocessor_error);

  private:
    // program counter
    uint64_t pc;
    uint64_t next_pc;

    // global instance
    static Cpu instance;

    // impl pattern
    class CpuImpl;
    class FpuImpl;
};

// https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Mips/Register.cpp#L9
constexpr std::array<std::string_view, 32> GPR_NAMES = {
    "R0", "AT", "V0", "V1", "A0", "A1", "A2", "A3", "T0", "T1", "T2",
    "T3", "T4", "T5", "T6", "T7", "S0", "S1", "S2", "S3", "S4", "S5",
    "S6", "S7", "T8", "T9", "K0", "K1", "GP", "SP", "FP", "RA",
};

} // namespace Cpu

inline Cpu::Cpu &g_cpu() { return Cpu::Cpu::get_instance(); }

} // namespace N64

#endif