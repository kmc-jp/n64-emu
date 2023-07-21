#ifndef PIF_H
#define PIF_H

#include "bus.h"
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

    switch (g_memory().rom.get_cic()) {
    case CicType::CIC_UNKNOWN:
        break;
    case CicType::CIC_NUS_6101:
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000000);
        g_cpu().gpr.write(2, 0xFFFFFFFFDF6445CCll);
        g_cpu().gpr.write(3, 0xFFFFFFFFDF6445CCll);
        g_cpu().gpr.write(4, 0x00000000000045CC);
        g_cpu().gpr.write(5, 0x0000000073EE317A);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08ll);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040ll);
        g_cpu().gpr.write(12, 0xFFFFFFFFC7601FACll);
        g_cpu().gpr.write(13, 0xFFFFFFFFC7601FACll);
        g_cpu().gpr.write(14, 0xFFFFFFFFB48E2ED6ll);
        g_cpu().gpr.write(15, 0xFFFFFFFFBA1A7D4Bll);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000001);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(23, 0x0000000000000001);
        g_cpu().gpr.write(24, 0x0000000000000002);
        g_cpu().gpr.write(25, 0xFFFFFFFF905F4718ll);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550ll);
        break;
    case CicType::CIC_NUS_7102:
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000001);
        g_cpu().gpr.write(2, 0x000000001E324416);
        g_cpu().gpr.write(3, 0x000000001E324416);
        g_cpu().gpr.write(4, 0x0000000000004416);
        g_cpu().gpr.write(5, 0x000000000EC5D9AF);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08ll);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040ll);
        g_cpu().gpr.write(12, 0x00000000495D3D7B);
        g_cpu().gpr.write(13, 0xFFFFFFFF8B3DFA1Ell);
        g_cpu().gpr.write(14, 0x000000004798E4D4);
        g_cpu().gpr.write(15, 0xFFFFFFFFF1D30682ll);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000000);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(22, 0x000000000000003F);
        g_cpu().gpr.write(23, 0x0000000000000007);
        g_cpu().gpr.write(24, 0x0000000000000000);
        g_cpu().gpr.write(25, 0x0000000013D05CAB);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001554ll);
        break;
    case CicType::CIC_NUS_6102_7101:
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000001);
        g_cpu().gpr.write(2, 0x000000000EBDA536);
        g_cpu().gpr.write(3, 0x000000000EBDA536);
        g_cpu().gpr.write(4, 0x000000000000A536);
        g_cpu().gpr.write(5, 0xFFFFFFFFC0F1D859ll);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08ll);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040ll);
        g_cpu().gpr.write(12, 0xFFFFFFFFED10D0B3ll);
        g_cpu().gpr.write(13, 0x000000001402A4CC);
        g_cpu().gpr.write(14, 0x000000002DE108EA);
        g_cpu().gpr.write(15, 0x000000003103E121);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000001);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(23, 0x0000000000000000);
        g_cpu().gpr.write(24, 0x0000000000000000);
        g_cpu().gpr.write(25, 0xFFFFFFFF9DEBB54Fll);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550ll);

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    case CicType::CIC_NUS_6103_7103:
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000001);
        g_cpu().gpr.write(2, 0x0000000049A5EE96);
        g_cpu().gpr.write(3, 0x0000000049A5EE96);
        g_cpu().gpr.write(4, 0x000000000000EE96);
        g_cpu().gpr.write(5, 0xFFFFFFFFD4646273ll);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08ll);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040ll);
        g_cpu().gpr.write(12, 0xFFFFFFFFCE9DFBF7ll);
        g_cpu().gpr.write(13, 0xFFFFFFFFCE9DFBF7ll);
        g_cpu().gpr.write(14, 0x000000001AF99984);
        g_cpu().gpr.write(15, 0x0000000018B63D28);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000001);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(23, 0x0000000000000000);
        g_cpu().gpr.write(24, 0x0000000000000000);
        g_cpu().gpr.write(25, 0xFFFFFFFF825B21C9ll);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550ll);

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    case CicType::CIC_NUS_6105_7105:
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000000);
        g_cpu().gpr.write(2, 0xFFFFFFFFF58B0FBFll);
        g_cpu().gpr.write(3, 0xFFFFFFFFF58B0FBFll);
        g_cpu().gpr.write(4, 0x0000000000000FBF);
        g_cpu().gpr.write(5, 0xFFFFFFFFDECAAAD1ll);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08ll);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040ll);
        g_cpu().gpr.write(12, 0xFFFFFFFF9651F81Ell);
        g_cpu().gpr.write(13, 0x000000002D42AAC5);
        g_cpu().gpr.write(14, 0x00000000489B52CF);
        g_cpu().gpr.write(15, 0x0000000056584D60);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000001);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(23, 0x0000000000000000);
        g_cpu().gpr.write(24, 0x0000000000000002);
        g_cpu().gpr.write(25, 0xFFFFFFFFCDCE565Fll);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550ll);

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554ll);
        }

        write_paddr32(PHYS_SPIMEM_BASE + 0x00, 0x3C0DBFC0);
        write_paddr32(PHYS_SPIMEM_BASE + 0x04, 0x8DA807FC);
        write_paddr32(PHYS_SPIMEM_BASE + 0x08, 0x25AD07C0);
        write_paddr32(PHYS_SPIMEM_BASE + 0x0C, 0x31080080);
        write_paddr32(PHYS_SPIMEM_BASE + 0x10, 0x5500FFFC);
        write_paddr32(PHYS_SPIMEM_BASE + 0x14, 0x3C0DBFC0);
        write_paddr32(PHYS_SPIMEM_BASE + 0x18, 0x8DA80024);
        write_paddr32(PHYS_SPIMEM_BASE + 0x1C, 0x3C0BB000);

        break;
    case CicType::CIC_NUS_6106_7106:
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000000);
        g_cpu().gpr.write(2, 0xFFFFFFFFA95930A4ll);
        g_cpu().gpr.write(3, 0xFFFFFFFFA95930A4ll);
        g_cpu().gpr.write(4, 0x00000000000030A4);
        g_cpu().gpr.write(5, 0xFFFFFFFFB04DC903ll);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0Cll);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08ll);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040ll);
        g_cpu().gpr.write(12, 0xFFFFFFFFBCB59510ll);
        g_cpu().gpr.write(13, 0xFFFFFFFFBCB59510ll);
        g_cpu().gpr.write(14, 0x000000000CF85C13);
        g_cpu().gpr.write(15, 0x000000007A3C07F4);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000001);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(23, 0x0000000000000000);
        g_cpu().gpr.write(24, 0x0000000000000002);
        g_cpu().gpr.write(25, 0x00000000465E3F72);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0ll);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550ll);

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554ll);
        }
        break;
    default:
        break;
    }

    // PCの初期化
    g_cpu().set_pc64(0xA4000040);

    // CPUのCOP0レジスタの初期化
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L305
    g_cpu().cop0.reg.random = 0x0000001F;
    g_cpu().cop0.reg.status.raw = 0x34000000;
    g_cpu().cop0.reg.prid = 0x00000B00;
    g_cpu().cop0.reg.config = 0x0006E463;

    // ROMの最初0x1000バイトをSP DMEMにコピー
    //   i.e. 0xB0000000 から 0xA4000000 に0x1000バイトをコピー
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L358
    memcpy(g_rsp().sp_dmem.data(), g_memory().rom.raw(), sizeof(uint8_t) * 0x1000);
}

} // namespace Memory
} // namespace N64

#endif