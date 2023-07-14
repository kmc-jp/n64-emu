#ifndef PIF_H
#define PIF_H

#include "cpu/cpu.h"
#include "rsp/rsp.h"

namespace N64 {
namespace Memory {

/* ROMのブートコード(PIF ROM)の副作用をエミュレートする */
// https://n64.readthedocs.io/#simulating-the-pif-rom
// FIXME: カートリッジの種類(CIC?)によって、PIF ROMの副作用が異なるっぽい
static void pif_rom_execute() {
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/PIF.cpp#L379
    bool pal{}; // TODO

    switch (n64mem.rom.get_cic()) {
    case CicType::CIC_UNKNOWN:
        break;
    case CicType::CIC_NUS_6101:
        n64cpu.gpr.write(0, 0x0000000000000000);
        n64cpu.gpr.write(1, 0x0000000000000000);
        n64cpu.gpr.write(2, 0xFFFFFFFFDF6445CCll);
        n64cpu.gpr.write(3, 0xFFFFFFFFDF6445CCll);
        n64cpu.gpr.write(4, 0x00000000000045CC);
        n64cpu.gpr.write(5, 0x0000000073EE317A);
        n64cpu.gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        n64cpu.gpr.write(7, 0xFFFFFFFFA4001F08ll);
        n64cpu.gpr.write(8, 0x00000000000000C0);
        n64cpu.gpr.write(9, 0x0000000000000000);
        n64cpu.gpr.write(10, 0x0000000000000040);
        n64cpu.gpr.write(11, 0xFFFFFFFFA4000040ll);
        n64cpu.gpr.write(12, 0xFFFFFFFFC7601FACll);
        n64cpu.gpr.write(13, 0xFFFFFFFFC7601FACll);
        n64cpu.gpr.write(14, 0xFFFFFFFFB48E2ED6ll);
        n64cpu.gpr.write(15, 0xFFFFFFFFBA1A7D4Bll);
        n64cpu.gpr.write(16, 0x0000000000000000);
        n64cpu.gpr.write(17, 0x0000000000000000);
        n64cpu.gpr.write(18, 0x0000000000000000);
        n64cpu.gpr.write(19, 0x0000000000000000);
        n64cpu.gpr.write(20, 0x0000000000000001);
        n64cpu.gpr.write(21, 0x0000000000000000);
        n64cpu.gpr.write(23, 0x0000000000000001);
        n64cpu.gpr.write(24, 0x0000000000000002);
        n64cpu.gpr.write(25, 0xFFFFFFFF905F4718ll);
        n64cpu.gpr.write(26, 0x0000000000000000);
        n64cpu.gpr.write(27, 0x0000000000000000);
        n64cpu.gpr.write(28, 0x0000000000000000);
        n64cpu.gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        n64cpu.gpr.write(30, 0x0000000000000000);
        n64cpu.gpr.write(31, 0xFFFFFFFFA4001550ll);
        break;
    case CicType::CIC_NUS_7102:
        n64cpu.gpr.write(0, 0x0000000000000000);
        n64cpu.gpr.write(1, 0x0000000000000001);
        n64cpu.gpr.write(2, 0x000000001E324416);
        n64cpu.gpr.write(3, 0x000000001E324416);
        n64cpu.gpr.write(4, 0x0000000000004416);
        n64cpu.gpr.write(5, 0x000000000EC5D9AF);
        n64cpu.gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        n64cpu.gpr.write(7, 0xFFFFFFFFA4001F08ll);
        n64cpu.gpr.write(8, 0x00000000000000C0);
        n64cpu.gpr.write(9, 0x0000000000000000);
        n64cpu.gpr.write(10, 0x0000000000000040);
        n64cpu.gpr.write(11, 0xFFFFFFFFA4000040ll);
        n64cpu.gpr.write(12, 0x00000000495D3D7B);
        n64cpu.gpr.write(13, 0xFFFFFFFF8B3DFA1Ell);
        n64cpu.gpr.write(14, 0x000000004798E4D4);
        n64cpu.gpr.write(15, 0xFFFFFFFFF1D30682ll);
        n64cpu.gpr.write(16, 0x0000000000000000);
        n64cpu.gpr.write(17, 0x0000000000000000);
        n64cpu.gpr.write(18, 0x0000000000000000);
        n64cpu.gpr.write(19, 0x0000000000000000);
        n64cpu.gpr.write(20, 0x0000000000000000);
        n64cpu.gpr.write(21, 0x0000000000000000);
        n64cpu.gpr.write(22, 0x000000000000003F);
        n64cpu.gpr.write(23, 0x0000000000000007);
        n64cpu.gpr.write(24, 0x0000000000000000);
        n64cpu.gpr.write(25, 0x0000000013D05CAB);
        n64cpu.gpr.write(26, 0x0000000000000000);
        n64cpu.gpr.write(27, 0x0000000000000000);
        n64cpu.gpr.write(28, 0x0000000000000000);
        n64cpu.gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        n64cpu.gpr.write(30, 0x0000000000000000);
        n64cpu.gpr.write(31, 0xFFFFFFFFA4001554ll);
        break;
    case CicType::CIC_NUS_6102_7101:
        n64cpu.gpr.write(0,   0x0000000000000000);
        n64cpu.gpr.write(1,   0x0000000000000001);
        n64cpu.gpr.write(2,   0x000000000EBDA536);
        n64cpu.gpr.write(3,   0x000000000EBDA536);
        n64cpu.gpr.write(4,   0x000000000000A536);
        n64cpu.gpr.write(5,   0xFFFFFFFFC0F1D859ll);
        n64cpu.gpr.write(6,   0xFFFFFFFFA4001F0Cll);
        n64cpu.gpr.write(7,   0xFFFFFFFFA4001F08ll);
        n64cpu.gpr.write(8,   0x00000000000000C0);
        n64cpu.gpr.write(9,   0x0000000000000000);
        n64cpu.gpr.write(10,  0x0000000000000040);
        n64cpu.gpr.write(11,  0xFFFFFFFFA4000040ll);
        n64cpu.gpr.write(12,  0xFFFFFFFFED10D0B3ll);
        n64cpu.gpr.write(13,  0x000000001402A4CC);
        n64cpu.gpr.write(14,  0x000000002DE108EA);
        n64cpu.gpr.write(15,  0x000000003103E121);
        n64cpu.gpr.write(16,  0x0000000000000000);
        n64cpu.gpr.write(17,  0x0000000000000000);
        n64cpu.gpr.write(18,  0x0000000000000000);
        n64cpu.gpr.write(19,  0x0000000000000000);
        n64cpu.gpr.write(20,  0x0000000000000001);
        n64cpu.gpr.write(21,  0x0000000000000000);
        n64cpu.gpr.write(23,  0x0000000000000000);
        n64cpu.gpr.write(24,  0x0000000000000000);
        n64cpu.gpr.write(25,  0xFFFFFFFF9DEBB54Fll);
        n64cpu.gpr.write(26,  0x0000000000000000);
        n64cpu.gpr.write(27,  0x0000000000000000);
        n64cpu.gpr.write(28,  0x0000000000000000);
        n64cpu.gpr.write(29,  0xFFFFFFFFA4001FF0ll);
        n64cpu.gpr.write(30,  0x0000000000000000);
        n64cpu.gpr.write(31,  0xFFFFFFFFA4001550ll);

        if (pal) {
            n64cpu.gpr.write(20, 0x0000000000000000);
            n64cpu.gpr.write(23, 0x0000000000000006);
            n64cpu.gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    case CicType::CIC_NUS_6103_7103:
        n64cpu.gpr.write(0,   0x0000000000000000);
        n64cpu.gpr.write(1,   0x0000000000000001);
        n64cpu.gpr.write(2,   0x0000000049A5EE96);
        n64cpu.gpr.write(3,   0x0000000049A5EE96);
        n64cpu.gpr.write(4,   0x000000000000EE96);
        n64cpu.gpr.write(5,   0xFFFFFFFFD4646273ll);
        n64cpu.gpr.write(6,   0xFFFFFFFFA4001F0Cll);
        n64cpu.gpr.write(7,   0xFFFFFFFFA4001F08ll);
        n64cpu.gpr.write(8,   0x00000000000000C0);
        n64cpu.gpr.write(9,   0x0000000000000000);
        n64cpu.gpr.write(10,  0x0000000000000040);
        n64cpu.gpr.write(11,  0xFFFFFFFFA4000040ll);
        n64cpu.gpr.write(12,  0xFFFFFFFFCE9DFBF7ll);
        n64cpu.gpr.write(13,  0xFFFFFFFFCE9DFBF7ll);
        n64cpu.gpr.write(14,  0x000000001AF99984);
        n64cpu.gpr.write(15,  0x0000000018B63D28);
        n64cpu.gpr.write(16,  0x0000000000000000);
        n64cpu.gpr.write(17,  0x0000000000000000);
        n64cpu.gpr.write(18,  0x0000000000000000);
        n64cpu.gpr.write(19,  0x0000000000000000);
        n64cpu.gpr.write(20,  0x0000000000000001);
        n64cpu.gpr.write(21,  0x0000000000000000);
        n64cpu.gpr.write(23,  0x0000000000000000);
        n64cpu.gpr.write(24,  0x0000000000000000);
        n64cpu.gpr.write(25,  0xFFFFFFFF825B21C9ll);
        n64cpu.gpr.write(26,  0x0000000000000000);
        n64cpu.gpr.write(27,  0x0000000000000000);
        n64cpu.gpr.write(28,  0x0000000000000000);
        n64cpu.gpr.write(29,  0xFFFFFFFFA4001FF0ll);
        n64cpu.gpr.write(30,  0x0000000000000000);
        n64cpu.gpr.write(31,  0xFFFFFFFFA4001550ll);

        if (pal) {
            n64cpu.gpr.write(20, 0x0000000000000000);
            n64cpu.gpr.write(23, 0x0000000000000006);
            n64cpu.gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    case CicType::CIC_NUS_6105_7105:
        n64cpu.gpr.write(0  , 0x0000000000000000);
        n64cpu.gpr.write(1  , 0x0000000000000000);
        n64cpu.gpr.write(2  , 0xFFFFFFFFF58B0FBFll);
        n64cpu.gpr.write(3  , 0xFFFFFFFFF58B0FBFll);
        n64cpu.gpr.write(4  , 0x0000000000000FBF);
        n64cpu.gpr.write(5  , 0xFFFFFFFFDECAAAD1ll);
        n64cpu.gpr.write(6  , 0xFFFFFFFFA4001F0Cll);
        n64cpu.gpr.write(7  , 0xFFFFFFFFA4001F08ll);
        n64cpu.gpr.write(8  , 0x00000000000000C0);
        n64cpu.gpr.write(9  , 0x0000000000000000);
        n64cpu.gpr.write(10 , 0x0000000000000040);
        n64cpu.gpr.write(11 , 0xFFFFFFFFA4000040ll);
        n64cpu.gpr.write(12 , 0xFFFFFFFF9651F81Ell);
        n64cpu.gpr.write(13 , 0x000000002D42AAC5);
        n64cpu.gpr.write(14 , 0x00000000489B52CF);
        n64cpu.gpr.write(15 , 0x0000000056584D60);
        n64cpu.gpr.write(16 , 0x0000000000000000);
        n64cpu.gpr.write(17 , 0x0000000000000000);
        n64cpu.gpr.write(18 , 0x0000000000000000);
        n64cpu.gpr.write(19 , 0x0000000000000000);
        n64cpu.gpr.write(20 , 0x0000000000000001);
        n64cpu.gpr.write(21 , 0x0000000000000000);
        n64cpu.gpr.write(23 , 0x0000000000000000);
        n64cpu.gpr.write(24 , 0x0000000000000002);
        n64cpu.gpr.write(25 , 0xFFFFFFFFCDCE565Fll);
        n64cpu.gpr.write(26 , 0x0000000000000000);
        n64cpu.gpr.write(27 , 0x0000000000000000);
        n64cpu.gpr.write(28 , 0x0000000000000000);
        n64cpu.gpr.write(29 , 0xFFFFFFFFA4001FF0ll);
        n64cpu.gpr.write(30 , 0x0000000000000000);
        n64cpu.gpr.write(31 , 0xFFFFFFFFA4001550ll);

        if (pal) {
            n64cpu.gpr.write(20, 0x0000000000000000);
            n64cpu.gpr.write(23, 0x0000000000000006);
            n64cpu.gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    case CicType::CIC_NUS_6106_7106:
        n64cpu.gpr.write(0,  0x0000000000000000);
        n64cpu.gpr.write(1,  0x0000000000000000);
        n64cpu.gpr.write(2,  0xFFFFFFFFA95930A4ll);
        n64cpu.gpr.write(3,  0xFFFFFFFFA95930A4ll);
        n64cpu.gpr.write(4,  0x00000000000030A4);
        n64cpu.gpr.write(5,  0xFFFFFFFFB04DC903ll);
        n64cpu.gpr.write(6,  0xFFFFFFFFA4001F0Cll);
        n64cpu.gpr.write(7,  0xFFFFFFFFA4001F08ll);
        n64cpu.gpr.write(8,  0x00000000000000C0);
        n64cpu.gpr.write(9,  0x0000000000000000);
        n64cpu.gpr.write(10,  0x0000000000000040);
        n64cpu.gpr.write(11,  0xFFFFFFFFA4000040ll);
        n64cpu.gpr.write(12,  0xFFFFFFFFBCB59510ll);
        n64cpu.gpr.write(13,  0xFFFFFFFFBCB59510ll);
        n64cpu.gpr.write(14,  0x000000000CF85C13);
        n64cpu.gpr.write(15,  0x000000007A3C07F4);
        n64cpu.gpr.write(16,  0x0000000000000000);
        n64cpu.gpr.write(17,  0x0000000000000000);
        n64cpu.gpr.write(18,  0x0000000000000000);
        n64cpu.gpr.write(19,  0x0000000000000000);
        n64cpu.gpr.write(20,  0x0000000000000001);
        n64cpu.gpr.write(21,  0x0000000000000000);
        n64cpu.gpr.write(23,  0x0000000000000000);
        n64cpu.gpr.write(24,  0x0000000000000002);
        n64cpu.gpr.write(25,  0x00000000465E3F72);
        n64cpu.gpr.write(26,  0x0000000000000000);
        n64cpu.gpr.write(27,  0x0000000000000000);
        n64cpu.gpr.write(28,  0x0000000000000000);
        n64cpu.gpr.write(29,  0xFFFFFFFFA4001FF0ll);
        n64cpu.gpr.write(30,  0x0000000000000000);
        n64cpu.gpr.write(31,  0xFFFFFFFFA4001550ll);

        if (pal) {
            n64cpu.gpr.write(20, 0x0000000000000000);
            n64cpu.gpr.write(23, 0x0000000000000006);
            n64cpu.gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    default:
        break;
    }

    // PCの初期化
    n64cpu.set_pc64(0xA4000040);

    // CPUのCOP0レジスタの初期化
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L305
    n64cpu.cop0.reg[Cpu::Cop0Reg::RANDOM] = 0x0000001F;
    n64cpu.cop0.reg[Cpu::Cop0Reg::STATUS] = 0x34000000;
    n64cpu.cop0.reg[Cpu::Cop0Reg::PRID] = 0x00000B00;
    n64cpu.cop0.reg[Cpu::Cop0Reg::CONFIG] = 0x0006E463;

    // ROMの最初0x1000バイトをSP DMEMにコピー
    //   i.e. 0xB0000000 から 0xA4000000 に0x1000バイトをコピー
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L358
    memcpy(n64rsp.sp_dmem, n64mem.rom.raw(), sizeof(uint8_t) * 0x1000);
}

} // namespace Memory
} // namespace N64

#endif