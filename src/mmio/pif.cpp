#include "mmio/pif.h"
#include "cpu/cpu.h"
#include "memory/bus.h"
#include "memory/memory.h"
#include "memory/memory_map.h"
#include "rcp/rsp.h"
#include <SDL.h>
#include <SDL_keyboard.h>
#include <cstdint>

namespace N64 {
namespace Mmio {

using Memory::CicType;

// https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/PIF.cpp#L338
static void rom_hle() {
    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/PIF.cpp#L379
    // FIXME: check PAL
    bool pal = false;

    // https://github.com/SimoneN64/Kaizen/blob/dffd36fc31731a0391a9b90f88ac2e5ed5d3f9ec/src/backend/core/mmio/PIF.cpp#L606
    const uint32_t cic_seed = g_memory().rom.get_cic_seed();
    N64::Memory::write_paddr32(PHYS_PIF_RAM_BASE + 0x24, cic_seed);

    switch (g_memory().rom.get_cic()) {
    case CicType::CIC_NUS_6101: {
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000000);
        g_cpu().gpr.write(2, 0xFFFFFFFFDF6445CC);
        g_cpu().gpr.write(3, 0xFFFFFFFFDF6445CC);
        g_cpu().gpr.write(4, 0x00000000000045CC);
        g_cpu().gpr.write(5, 0x0000000073EE317A);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0C);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040);
        g_cpu().gpr.write(12, 0xFFFFFFFFC7601FAC);
        g_cpu().gpr.write(13, 0xFFFFFFFFC7601FAC);
        g_cpu().gpr.write(14, 0xFFFFFFFFB48E2ED6);
        g_cpu().gpr.write(15, 0xFFFFFFFFBA1A7D4B);
        g_cpu().gpr.write(16, 0x0000000000000000);
        g_cpu().gpr.write(17, 0x0000000000000000);
        g_cpu().gpr.write(18, 0x0000000000000000);
        g_cpu().gpr.write(19, 0x0000000000000000);
        g_cpu().gpr.write(20, 0x0000000000000001);
        g_cpu().gpr.write(21, 0x0000000000000000);
        g_cpu().gpr.write(23, 0x0000000000000001);
        g_cpu().gpr.write(24, 0x0000000000000002);
        g_cpu().gpr.write(25, 0xFFFFFFFF905F4718);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550);

        g_cpu().lo = 0xFFFFFFFFBA1A7D4B;
        g_cpu().hi = 0xFFFFFFFF997EC317;
    } break;
    case CicType::CIC_NUS_7102: {
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000001);
        g_cpu().gpr.write(2, 0x000000001E324416);
        g_cpu().gpr.write(3, 0x000000001E324416);
        g_cpu().gpr.write(4, 0x0000000000004416);
        g_cpu().gpr.write(5, 0x000000000EC5D9AF);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0C);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040);
        g_cpu().gpr.write(12, 0x00000000495D3D7B);
        g_cpu().gpr.write(13, 0xFFFFFFFF8B3DFA1E);
        g_cpu().gpr.write(14, 0x000000004798E4D4);
        g_cpu().gpr.write(15, 0xFFFFFFFFF1D30682);
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
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001554);

        g_cpu().lo = 0xFFFFFFFFF1D30682;
        g_cpu().hi = 0x0000000010054A98;
    } break;
    case CicType::CIC_NUS_6102_7101: {
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000001);
        g_cpu().gpr.write(2, 0x000000000EBDA536);
        g_cpu().gpr.write(3, 0x000000000EBDA536);
        g_cpu().gpr.write(4, 0x000000000000A536);
        g_cpu().gpr.write(5, 0xFFFFFFFFC0F1D859);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0C);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040);
        g_cpu().gpr.write(12, 0xFFFFFFFFED10D0B3);
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
        g_cpu().gpr.write(25, 0xFFFFFFFF9DEBB54F);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550);

        g_cpu().hi = 0x000000003FC18657;
        g_cpu().lo = 0x000000003103E121;

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554);
        }
    } break;
    case CicType::CIC_NUS_6103_7103: {
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000001);
        g_cpu().gpr.write(2, 0x0000000049A5EE96);
        g_cpu().gpr.write(3, 0x0000000049A5EE96);
        g_cpu().gpr.write(4, 0x000000000000EE96);
        g_cpu().gpr.write(5, 0xFFFFFFFFD4646273);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0C);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040);
        g_cpu().gpr.write(12, 0xFFFFFFFFCE9DFBF7);
        g_cpu().gpr.write(13, 0xFFFFFFFFCE9DFBF7);
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
        g_cpu().gpr.write(25, 0xFFFFFFFF825B21C9);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550);

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554);
        }

        g_cpu().lo = 0x0000000018B63D28;
        g_cpu().hi = 0x00000000625C2BBE;
    } break;
    case CicType::CIC_NUS_6105_7105: {
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000000);
        g_cpu().gpr.write(2, 0xFFFFFFFFF58B0FBF);
        g_cpu().gpr.write(3, 0xFFFFFFFFF58B0FBF);
        g_cpu().gpr.write(4, 0x0000000000000FBF);
        g_cpu().gpr.write(5, 0xFFFFFFFFDECAAAD1);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0C);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040);
        g_cpu().gpr.write(12, 0xFFFFFFFF9651F81E);
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
        g_cpu().gpr.write(25, 0xFFFFFFFFCDCE565F);
        g_cpu().gpr.write(26, 0x0000000000000000);
        g_cpu().gpr.write(27, 0x0000000000000000);
        g_cpu().gpr.write(28, 0x0000000000000000);
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550);

        g_cpu().lo = 0x0000000056584D60;
        g_cpu().hi = 0x000000004BE35D1F;

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554);
        }

        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x00, 0x3C0DBFC0);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x04, 0x8DA807FC);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x08, 0x25AD07C0);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x0C, 0x31080080);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x10, 0x5500FFFC);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x14, 0x3C0DBFC0);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x18, 0x8DA80024);
        Memory::write_paddr32(PHYS_SPIMEM_BASE + 0x1C, 0x3C0BB000);

    } break;
    case CicType::CIC_NUS_6106_7106: {
        g_cpu().gpr.write(0, 0x0000000000000000);
        g_cpu().gpr.write(1, 0x0000000000000000);
        g_cpu().gpr.write(2, 0xFFFFFFFFA95930A4);
        g_cpu().gpr.write(3, 0xFFFFFFFFA95930A4);
        g_cpu().gpr.write(4, 0x00000000000030A4);
        g_cpu().gpr.write(5, 0xFFFFFFFFB04DC903);
        g_cpu().gpr.write(6, 0xFFFFFFFFA4001F0C);
        g_cpu().gpr.write(7, 0xFFFFFFFFA4001F08);
        g_cpu().gpr.write(8, 0x00000000000000C0);
        g_cpu().gpr.write(9, 0x0000000000000000);
        g_cpu().gpr.write(10, 0x0000000000000040);
        g_cpu().gpr.write(11, 0xFFFFFFFFA4000040);
        g_cpu().gpr.write(12, 0xFFFFFFFFBCB59510);
        g_cpu().gpr.write(13, 0xFFFFFFFFBCB59510);
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
        g_cpu().gpr.write(29, 0xFFFFFFFFA4001FF0);
        g_cpu().gpr.write(30, 0x0000000000000000);
        g_cpu().gpr.write(31, 0xFFFFFFFFA4001550);

        g_cpu().lo = 0x000000007A3C07F4;
        g_cpu().hi = 0x0000000023953898;

        if (pal) {
            g_cpu().gpr.write(20, 0x0000000000000000);
            g_cpu().gpr.write(23, 0x0000000000000006);
            g_cpu().gpr.write(31, 0xFFFFFFFFA4001554);
        }
    } break;
    default: {
        Utils::abort("Could not boot. Unsupported CIC type.");
    } break;
    }

    g_cpu().gpr.write(22, (cic_seed >> 8) & 0xFF);

    // PCの初期化
    g_cpu().set_pc64(0xA4000040);

    // CPUのCOP0レジスタの初期化
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L305
    g_cpu().cop0.reg.random = 0x0000001F;
    g_cpu().cop0.reg.status.raw = 0x34000000;
    g_cpu().cop0.reg.prid = 0x00000B00;
    g_cpu().cop0.reg.config = 0x0006E463;

    // FIXME: correct?
    N64::Memory::write_paddr32(0x04300004, 0x01010101);

    // ROMの最初0x1000バイトをSP DMEMにコピー
    //   i.e. 0xB0000000 から 0xA4000000 に0x1000バイトをコピー
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/mem/pif.c#L358
    memcpy(g_rsp().get_sp_dmem().data(), g_memory().rom.get_raw_data().data(),
           sizeof(uint8_t) * 0x1000);
}

// ROMのブートコード(PIF ROM)の副作用をエミュレートする
void Pif::execute_rom_hle() {

    switch (g_memory().rom.get_cic()) {
    case CicType::CIC_UNKNOWN: {
        Utils::abort("Unknown CIC type");
    } break;
    case CicType::CIC_NUS_6101:
    case CicType::CIC_NUS_7102:
    case CicType::CIC_NUS_6102_7101:
    case CicType::CIC_NUS_6103_7103: {
        N64::Memory::write_paddr32(0x318, RDRAM_SIZE);
    } break;
    case CicType::CIC_NUS_6105_7105: {
        N64::Memory::write_paddr32(0x3F0, RDRAM_SIZE);
    } break;
    case CicType::CIC_NUS_6106_7106:
        break;
    }
    rom_hle();
}

void Pif::control_write() {
    Utils::debug("PIF: control_write");
    // The last byte of PIF RAM is called 'control' (or 'command'). The control
    // byte contains bit flags representings a command performed by PIF.
    // See: https://n64brew.dev/wiki/PIF-NUS
    uint8_t control = ram[63];
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PifRamHandler.cpp#L377
    if (control > 1) {
        switch (control) {
        case 0x08: // Terminate boot process.
            // PIF expects this command is sent before 5 seconds from boot.
            // Nothing to emulate.
            break;
        default: {
            Utils::critical("control = {:#b}", control);
            Utils::unimplemented("Abort");
        } break;
        }
    }

    // Run Joy bus commands (64 bytes)
    // https://github.com/SimoneN64/Kaizen/blob/74dccb6ac6a679acbf41b497151e08af6302b0e9/src/backend/core/mmio/PIF.cpp#L155
    // https://github.com/project64/project64/blob/353ef5ed897cb72a8904603feddbdc649dff9eca/Source/Project64-core/N64System/MemoryHandler/PifRamHandler.cpp#L594
    int channel = 0;

    // For details of command,
    // see: https://n64brew.dev/wiki/Joybus_Protocol#Command_Details
    for (int cursor = 0; cursor < 64; cursor++) {
        Utils::debug("Joybus: channel = {}, cursor = {}, ram[cursor] = {}",
                     channel, cursor, ram[cursor]);
        switch (ram[cursor]) {
        case 0x00: {
            channel++;
            if (channel > 6)
                cursor = 0x40;
        } break;
        case 0xFD: // fallthrough
        case 0xFE: {
            cursor = 0x40;
        } break;
        case 0xFF: // fallthrough
        case 0xB4: // fallthrough
        case 0x56: // fallthrough
        case 0xB8:
            break;
        default: {
            if (channel < 4) {
                process_controller_command(channel, &ram[cursor]);
            } else {
                Utils::critical("PIF: channel = {} >= 4", channel);
                Utils::unimplemented("Aborted");
            }
            // cursor advances by command length + command result length
            cursor += ram[cursor] + (ram[cursor + 1] & 0x3F) + 1;
            channel++;
        } break;
        }
    }
}

// https://n64brew.dev/wiki/Joybus_Protocol#Command_Details
void Pif::process_controller_command(int channel, uint8_t *cmd) {
    switch (cmd[2]) {
    case 0x00: // Info. fallthrough
    case 0xFF: // Reset/Info
    {
        // TODO: (controller) Add more kinds of joypad
        switch (joycon_type) {
        case JoyBusControllerType::NONE: {
            // FIXME: nothing to do? I am not sure.
        } break;
        case JoyBusControllerType::N64_CONTROLLER: {
            cmd[3] = 0x05;
            cmd[4] = 0x00;
        } break;
        default: {
            Utils::abort("Unsupported controller type!");
        } break;
        }
        switch (joycon_plugin) {
        case JoyBusControllerPlugin::TRANSFER_PAK:
        case JoyBusControllerPlugin::RUMBLE_PAK:
        case JoyBusControllerPlugin::MEM_PAK:
        case JoyBusControllerPlugin::RAW: {
            // FIXME: correct?
            cmd[5] = 1;
        } break;
        default: {
            Utils::abort("Unsupported controller plugin");
        } break;
        }
    } break;
    case 0x01: // Read controller
    {
        switch (joycon_type) {
        case JoyBusControllerType::NONE: {
            cmd[3] = 0;
            cmd[4] = 0;
            cmd[5] = 0;
            cmd[6] = 0;
        } break;
        case JoyBusControllerType::N64_CONTROLLER: {
            const N64ControllerState s = poll_n64_controller();
            cmd[3] = s.byte1;
            cmd[4] = s.byte2;
            cmd[5] = s.joy_x;
            cmd[6] = s.joy_y;
        } break;
        default: {
            Utils::abort(
                "Unsupported controller type. Could not read buttons.");
        }
        }
    } break;
    default: {
        Utils::critical("Unknown controller command: {}", cmd[2]);
        Utils::abort("Aborted");
    } break;
    }
}

N64ControllerState Pif::poll_n64_controller() const {
    N64ControllerState ret;
    // TODO: Support multiple controllers. (Use controller channel)
    SDL_PumpEvents();
    const uint8_t *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_PAGEUP])
        ret.byte2 |= N64ControllerByte2::C_UP;
    if (state[SDL_SCANCODE_PAGEDOWN])
        ret.byte2 |= N64ControllerByte2::C_DOWN;
    if (state[SDL_SCANCODE_HOME])
        ret.byte2 |= N64ControllerByte2::C_LEFT;
    if (state[SDL_SCANCODE_END])
        ret.byte2 |= N64ControllerByte2::C_RIGHT;

    if (state[SDL_SCANCODE_W])
        ret.byte1 |= N64ControllerByte1::DP_UP;
    if (state[SDL_SCANCODE_S])
        ret.byte1 |= N64ControllerByte1::DP_DOWN;
    if (state[SDL_SCANCODE_A])
        ret.byte1 |= N64ControllerByte1::DP_LEFT;
    if (state[SDL_SCANCODE_D])
        ret.byte1 |= N64ControllerByte1::DP_RIGHT;

    Utils::critical("byte1 {:#10b}", ret.byte1);
    Utils::critical("byte2 {:#10b}", ret.byte2);
    Utils::critical("joy_x {:#10b}", ret.joy_x);
    Utils::critical("joy_y {:#10b}", ret.joy_y);

    // TODO: read more buttons
    /*
    ret.a = 0;
    ret.b = 0;
    ret.z = 0;
    ret.l = 0;
    ret.r = 0;
    ret.zero = 0;
    ret.start = 0;
    ret.joy_reset = 0;
    ret.joy_x = 0;
    ret.joy_y = 0;
    */
    return ret;
}

} // namespace Mmio
} // namespace N64
