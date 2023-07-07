#ifndef PIF_H
#define PIF_H

#include "cpu/cpu.h"

namespace N64 {

extern Cpu n64cpu;

/* ROMのブートコード(PIF ROM)の副作用をエミュレートする */
static void pif_rom_execute() {
    // https://n64.readthedocs.io/#simulating-the-pif-rom
    n64cpu.gpr[11] = 0xFFFFFFFF'A4000040;
    n64cpu.gpr[20] = 0x00000000'00000001;
    n64cpu.gpr[22] = 0x00000000'0000003F;
    n64cpu.gpr[29] = 0xFFFFFFFF'A4001FF0;
    
    // TODO: COP0レジスタのセット
    
    // TODO: ROMの最初0x1000バイトをSP DMEMにコピー
    //   i.e. 0xB0000000 から 0xA4000000 に0x1000バイトをコピー
    
    n64cpu.pc = 0xA4000040;
}

} // namespace N64

#endif
