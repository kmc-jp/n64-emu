#include "rdp/rdp_core.h"
#include "rdp_device.hpp"
#include "utils/log.h"
#include <mutex>

namespace N64 {
namespace Rdp {

namespace {
constexpr uint32_t RDRAM_SIZE = 8 * 1024 * 1024;
constexpr uint32_t HIDDEN_RDRAM_SIZE = 4 * 1024 * 1024;

RDP::CommandProcessor *g_command_processor = nullptr;
bool g_rdp_dirty = true;

constexpr RDP::ScanoutOptions scanout_options() {
    RDP::ScanoutOptions opts;
    opts.persist_frame_on_invalid_input = true;
    opts.vi.aa = true;
    opts.vi.scale = true;
    opts.vi.dither_filter = true;
    opts.vi.divot_filter = true;
    opts.vi.gamma_dither = true;
    opts.downscale_steps = 0;
    opts.crop_overscan_pixels = 0;
    return opts;
}
} // namespace

std::recursive_mutex &mutex() {
    static std::recursive_mutex mu;
    return mu;
}

void init(Vulkan::Device &device, uint8_t *rdram, unsigned upscale) {
    std::lock_guard lock(mutex());
    g_rdp_dirty = true;

    RDP::CommandProcessorFlags flags = 0;
    switch (upscale) {
    case 2:
        flags = RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_2X_BIT;
        break;
    case 4:
        flags = RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_4X_BIT;
        break;
    case 8:
        flags = RDP::COMMAND_PROCESSOR_FLAG_UPSCALING_8X_BIT;
        break;
    case 1:
        break;
    default:
        Utils::warn("Unsupported RDP upscale factor {}; using 1x", upscale);
        break;
    }

    auto aligned_rdram = reinterpret_cast<uintptr_t>(rdram);
    uintptr_t offset = 0;
    if (device.get_device_features().supports_external_memory_host) {
        size_t align = device.get_device_features()
                           .host_memory_properties.minImportedHostPointerAlignment;
        offset = aligned_rdram & (align - 1);
        aligned_rdram -= offset;
    }

    delete g_command_processor;
    g_command_processor = new RDP::CommandProcessor(
        device, reinterpret_cast<void *>(aligned_rdram), offset, RDRAM_SIZE,
        HIDDEN_RDRAM_SIZE, flags);

    if (!g_command_processor->device_is_supported()) {
        Utils::critical("Parallel-RDP does not support this device. Sorry!");
        exit(-1);
    }
}

void fini() {
    std::lock_guard lock(mutex());
    if (g_command_processor)
        g_command_processor->set_sync_full_callback(nullptr, nullptr);
    delete g_command_processor;
    g_command_processor = nullptr;
    g_rdp_dirty = true;
}

bool ready() { return g_command_processor != nullptr; }

void enqueue_command(int command_length, const uint32_t *buffer) {
    std::lock_guard lock(mutex());
    if (!g_command_processor)
        return;
    g_rdp_dirty = true;
    g_command_processor->enqueue_command(command_length, buffer);
}

void on_full_sync() {
    std::lock_guard lock(mutex());
    if (!g_command_processor)
        return;
    g_command_processor->wait_for_timeline(g_command_processor->signal_timeline());
}

void set_sync_full_callback(SyncFullCallback cb, void *userdata) {
    std::lock_guard lock(mutex());
    if (!g_command_processor)
        return;
    g_command_processor->set_sync_full_callback(cb, userdata);
}

bool is_dirty() {
    std::lock_guard lock(mutex());
    return g_rdp_dirty;
}

void clear_dirty() {
    std::lock_guard lock(mutex());
    g_rdp_dirty = false;
}

ScanoutResult scanout(const ViRegs &vi) {
    std::lock_guard lock(mutex());
    ScanoutResult out;
    if (!g_command_processor)
        return out;

    uint32_t h_video = vi.h_video;
    const uint32_t h_start = (h_video >> 16) & 0x3ff;
    const uint32_t h_end = h_video & 0x3ff;
    if (h_start == 0 && h_end == 0 && vi.width != 0 && vi.origin != 0 &&
        vi.origin != 0x280) {
        static bool logged = false;
        if (!logged) {
            Utils::warn(
                "VI_H_VIDEO is blank (0); using NTSC default 0x006c02ec "
                "for scanout (origin={:#x})",
                vi.origin);
            logged = true;
        }
        h_video = 0x006c02ec;
    }

    g_command_processor->set_vi_register(RDP::VIRegister::Control, vi.status);
    g_command_processor->set_vi_register(RDP::VIRegister::Origin, vi.origin);
    g_command_processor->set_vi_register(RDP::VIRegister::Width, vi.width);
    g_command_processor->set_vi_register(RDP::VIRegister::Intr, vi.intr);
    g_command_processor->set_vi_register(RDP::VIRegister::VCurrentLine,
                                        vi.current);
    g_command_processor->set_vi_register(RDP::VIRegister::Timing, vi.burst);
    g_command_processor->set_vi_register(RDP::VIRegister::VSync, vi.vsync);
    g_command_processor->set_vi_register(RDP::VIRegister::HSync, vi.hsync);
    g_command_processor->set_vi_register(RDP::VIRegister::Leap, vi.hsync_leap);
    g_command_processor->set_vi_register(RDP::VIRegister::HStart, h_video);
    g_command_processor->set_vi_register(RDP::VIRegister::VStart, vi.v_video);
    g_command_processor->set_vi_register(RDP::VIRegister::VBurst, vi.v_burst);
    g_command_processor->set_vi_register(RDP::VIRegister::XScale, vi.x_scale);
    g_command_processor->set_vi_register(RDP::VIRegister::YScale, vi.y_scale);

    out.origin = vi.origin;
    if (vi.origin == 0 || vi.origin == 0x280) {
        out.skip = true;
        return out;
    }

    out.image = g_command_processor->scanout(scanout_options());
    out.skip = false;
    return out;
}

} // namespace Rdp
} // namespace N64
