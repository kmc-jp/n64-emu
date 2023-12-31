#include "fpu_instruction_impl.h"
#include "cpu/cpu.h"
#include "utils/log.h"

namespace N64::Cpu {

bool FpuImpl::test_cop1_usable_exception(Cpu &cpu) {
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Interpreter/InterpreterOps.cpp#L3071
    if (cpu.cop0.reg.status.cu1 == 0) {
        // TODO: implement
        Utils::unimplemented("Trigger cop1 exception");
        return true;
    }
    return false;
}

void FpuImpl::op_cfc1(Cpu &cpu, instruction_t inst) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/fpu_instructions.c#L315
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Interpreter/InterpreterOps.cpp#L1917
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    int32_t value;
    switch (fs) {
    case 0: {
        value = cpu.cop1.fcr0;
    } break;
    case 31: {
        value = cpu.cop1.fcr31.raw;
    } break;
    default: {
        Utils::abort("CFC1: Unknown FS: {}", fs);
    } break;
    }
    cpu.gpr.write(inst.r_type.rt, (int64_t)value);
    Utils::instruction_trace("CFC1 FCR[{}], {}", fs, GPR_NAMES[inst.r_type.rt]);
}

void FpuImpl::op_ctc1(Cpu &cpu, instruction_t inst) {
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/cpu/fpu_instructions.c#L334
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/Interpreter/InterpreterOps.cpp#L1944
    if (test_cop1_usable_exception(cpu)) {
        return;
    }
    uint8_t fs = inst.r_type.rd;
    uint32_t value = cpu.gpr.read(inst.r_type.rt);
    switch (fs) {
    case 0: {
        Utils::abort("fcr0 is read only");
    } break;
    case 31: {
        value &= 0x183ffff;
        cpu.cop1.fcr31.raw = value;
    } break;
    default: {
        Utils::abort("CTC1: Unknown FS: {}", fs);
    } break;
    }
    Utils::trace("CTC1 FCR[{}], {}", fs, GPR_NAMES[inst.r_type.rt]);
}

} // namespace N64::Cpu
