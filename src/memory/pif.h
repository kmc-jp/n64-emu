#ifndef PIF_H
#define PIF_H

#include "cpu/cpu.h"
#include "rsp/rsp.h"

namespace N64 {

extern Cpu n64cpu;

/* ROMのブートコード(PIF ROM)の副作用をエミュレートする */
// https://n64.readthedocs.io/#simulating-the-pif-rom
// FIXME: カートリッジの種類(CIC?)によって、PIF ROMの副作用が異なるっぽい
static void pif_rom_execute() { // CPUのGPRの初期化
    n64cpu.gprs[11] = 0xFFFFFFFF'A4000040;
    n64cpu.gprs[20] = 0x00000000'00000001;
    n64cpu.gprs[22] = 0x00000000'0000003F;
    n64cpu.gprs[29] = 0xFFFFFFFF'A4001FF0;
    // PCの初期化
    n64cpu.pc = 0xA4000040;

    // CPUのCOP0レジスタの初期化
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L305
    n64cpu.cop0.regs[Cop0Reg::RANDOM] = 0x0000001F;
    n64cpu.cop0.regs[Cop0Reg::STATUS] = 0x34000000;
    n64cpu.cop0.regs[Cop0Reg::PRID] = 0x00000B00;
    n64cpu.cop0.regs[Cop0Reg::CONFIG] = 0x0006E463;

    // ROMの最初0x1000バイトをSP DMEMにコピー
    //   i.e. 0xB0000000 から 0xA4000000 に0x1000バイトをコピー
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L358
    memcpy(n64rsp.sp_dmem, n64mem.rom.raw(), sizeof(uint8_t) * 0x1000);
}

} // namespace N64

#endif
