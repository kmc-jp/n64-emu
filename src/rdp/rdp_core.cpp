#include "rdp/rdp_core.h"
#include "memory/memory_map.h"
#include "mmio/vi.h"
#include "rdp_device.hpp"
#include "utils/log.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace N64 {
namespace Rdp {

namespace {
constexpr uint32_t HIDDEN_RDRAM_SIZE = 4 * 1024 * 1024;

RDP::CommandProcessor *g_command_processor = nullptr;
bool g_rdp_dirty = true;
// Set when CPU/DMA writes hit the active VI framebuffer. Software renderers
// keep a fixed VI_ORIGIN, so present must not treat those frames as duplicates.
std::atomic<bool> g_cpu_fb_dirty{false};

std::atomic<uint64_t> g_sync_signal{0};
std::vector<uint8_t> g_rdram_dirty;

struct FrameBufferInfo {
    uint32_t framebuffer_address = 0;
    uint32_t framebuffer_pixel_size = 0;
    uint32_t framebuffer_width = 0;
    uint32_t framebuffer_height = 0;
    uint32_t framebuffer_y_offset = 0;
    uint32_t depthbuffer_address = 0;
    bool depth_buffer_enabled = false;
    uint32_t texture_address = 0;
    uint32_t texture_pixel_size = 0;
    uint32_t texture_width = 0;
} g_fb_info;

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

uint32_t pixel_bytes(uint32_t pixel_type, uint32_t area) {
    switch (pixel_type) {
    case 0:
        return area / 2;
    case 1:
        return area;
    case 2:
        return area * 2;
    case 3:
        return area * 4;
    default:
        return 0;
    }
}

void mark_dirty_range(uint32_t address, uint32_t length) {
    if (length == 0 || g_rdram_dirty.empty())
        return;
    uint32_t start = address >> 3;
    uint32_t end =
        (address + length + 7) >> 3;
    if (start >= g_rdram_dirty.size())
        return;
    end = std::min(end, static_cast<uint32_t>(g_rdram_dirty.size()));
    if (g_rdram_dirty[start])
        return;
    std::fill(g_rdram_dirty.begin() + start, g_rdram_dirty.begin() + end,
              uint8_t{1});
}

void mark_color_depth_dirty() {
    const uint32_t fb_off = pixel_bytes(
        g_fb_info.framebuffer_pixel_size,
        g_fb_info.framebuffer_y_offset * g_fb_info.framebuffer_width);
    const uint32_t fb_addr = g_fb_info.framebuffer_address + fb_off;
    const uint32_t fb_len = pixel_bytes(
        g_fb_info.framebuffer_pixel_size,
        g_fb_info.framebuffer_width * g_fb_info.framebuffer_height);
    mark_dirty_range(fb_addr, fb_len);

    if (g_fb_info.depth_buffer_enabled) {
        const uint32_t zb_off = pixel_bytes(
            2, g_fb_info.framebuffer_y_offset * g_fb_info.framebuffer_width);
        const uint32_t zb_addr = g_fb_info.depthbuffer_address + zb_off;
        const uint32_t zb_len = pixel_bytes(
            2, g_fb_info.framebuffer_width * g_fb_info.framebuffer_height);
        mark_dirty_range(zb_addr, zb_len);
    }
}

void track_command(const uint32_t *words) {
    const uint32_t w1 = words[0];
    const uint32_t w2 = words[1];
    const uint32_t command = (w1 >> 24) & 0x3F;

    switch (static_cast<RDP::Op>(command)) {
    case RDP::Op::FillTriangle:
    case RDP::Op::FillZBufferTriangle:
    case RDP::Op::TextureTriangle:
    case RDP::Op::TextureZBufferTriangle:
    case RDP::Op::ShadeTriangle:
    case RDP::Op::ShadeZBufferTriangle:
    case RDP::Op::ShadeTextureTriangle:
    case RDP::Op::ShadeTextureZBufferTriangle:
    case RDP::Op::TextureRectangle:
    case RDP::Op::TextureRectangleFlip:
    case RDP::Op::FillRectangle:
        mark_color_depth_dirty();
        break;
    case RDP::Op::LoadTLut:
    case RDP::Op::LoadTile: {
        const uint32_t upper_left_t = (w1 & 0xFFF) >> 2;
        const uint32_t lower_right_t = (w2 & 0xFFF) >> 2;
        const uint32_t addr =
            g_fb_info.texture_address +
            pixel_bytes(g_fb_info.texture_pixel_size,
                        upper_left_t * g_fb_info.texture_width);
        const uint32_t len = pixel_bytes(
            g_fb_info.texture_pixel_size,
            (lower_right_t - upper_left_t) * g_fb_info.texture_width);
        mark_dirty_range(addr, len);
        break;
    }
    case RDP::Op::LoadBlock: {
        const uint32_t upper_left_s = (w1 >> 12) & 0xFFF;
        const uint32_t upper_left_t = w1 & 0xFFF;
        const uint32_t lower_right_s = (w2 >> 12) & 0xFFF;
        const uint32_t addr =
            g_fb_info.texture_address +
            pixel_bytes(g_fb_info.texture_pixel_size,
                        upper_left_s +
                            upper_left_t * g_fb_info.texture_width);
        const uint32_t len =
            pixel_bytes(g_fb_info.texture_pixel_size,
                        lower_right_s > upper_left_s
                            ? lower_right_s - upper_left_s
                            : 0);
        mark_dirty_range(addr, len);
        break;
    }
    case RDP::Op::SetColorImage:
        g_fb_info.framebuffer_address = w2 & 0x00FFFFFF;
        g_fb_info.framebuffer_pixel_size = (w1 >> 19) & 0x3;
        g_fb_info.framebuffer_width = (w1 & 0x3FF) + 1;
        break;
    case RDP::Op::SetMaskImage:
        g_fb_info.depthbuffer_address = w2 & 0x00FFFFFF;
        break;
    case RDP::Op::SetTextureImage:
        g_fb_info.texture_address = w2 & 0x00FFFFFF;
        g_fb_info.texture_pixel_size = (w1 >> 19) & 0x3;
        g_fb_info.texture_width = (w1 & 0x3FF) + 1;
        break;
    case RDP::Op::SetScissor: {
        const uint32_t upper_left_y = (w1 & 0xFFF) >> 2;
        const uint32_t lower_right_y = (w2 & 0xFFF) >> 2;
        g_fb_info.framebuffer_y_offset = upper_left_y;
        g_fb_info.framebuffer_height =
            lower_right_y > upper_left_y ? lower_right_y - upper_left_y : 0;
        break;
    }
    case RDP::Op::SetOtherModes: {
        const uint8_t cycle_type = (w1 >> 20) & 3;
        const uint8_t depth_read_write = (w2 >> 4) & 3;
        g_fb_info.depth_buffer_enabled =
            ((cycle_type & 2) == 0) && (depth_read_write != 0);
        break;
    }
    default:
        break;
    }
}

void reset_deferred_sync_state() {
    g_sync_signal.store(0, std::memory_order_relaxed);
    g_rdram_dirty.assign(RDRAM_SIZE >> 3, 0);
    g_fb_info = {};
}

void flush_pending_sync_locked() {
    const uint64_t signal = g_sync_signal.load(std::memory_order_relaxed);
    if (!signal || !g_command_processor)
        return;
    g_command_processor->wait_for_timeline(signal);
    std::fill(g_rdram_dirty.begin(), g_rdram_dirty.end(), uint8_t{0});
    g_sync_signal.store(0, std::memory_order_release);
}
} // namespace

std::recursive_mutex &mutex() {
    static std::recursive_mutex mu;
    return mu;
}

void init(Vulkan::Device &device, uint8_t *rdram, unsigned upscale) {
    std::lock_guard lock(mutex());
    g_rdp_dirty = true;
    g_cpu_fb_dirty.store(true, std::memory_order_relaxed);
    reset_deferred_sync_state();

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
    g_cpu_fb_dirty.store(true, std::memory_order_relaxed);
    reset_deferred_sync_state();
}

bool ready() { return g_command_processor != nullptr; }

void enqueue_command(int command_length, const uint32_t *buffer) {
    std::lock_guard lock(mutex());
    if (!g_command_processor)
        return;
    g_rdp_dirty = true;
    if (command_length >= 2)
        track_command(buffer);
    g_command_processor->enqueue_command(command_length, buffer);
}

void on_full_sync() {
    std::lock_guard lock(mutex());
    if (!g_command_processor)
        return;
    const uint64_t signal = g_command_processor->signal_timeline();
    g_sync_signal.store(signal, std::memory_order_release);
}

void check_framebuffers(uint32_t address, uint32_t length) {
    // Hot path: every RDRAM load/store after FULL_SYNC. Acquire on the signal
    // synchronizes with on_full_sync's release, so dirty bits written earlier
    // under the RDP mutex are visible without taking the lock on misses.
    if (g_sync_signal.load(std::memory_order_acquire) == 0)
        return;
    if (length == 0 || g_rdram_dirty.empty())
        return;

    uint32_t start = address >> 3;
    uint32_t end = (address + length + 7) >> 3;
    if (start >= g_rdram_dirty.size())
        return;
    end = std::min(end, static_cast<uint32_t>(g_rdram_dirty.size()));

    const auto it =
        std::find(g_rdram_dirty.begin() + start, g_rdram_dirty.begin() + end,
                  uint8_t{1});
    if (it == g_rdram_dirty.begin() + end)
        return;

    std::lock_guard lock(mutex());
    if (g_sync_signal.load(std::memory_order_relaxed) == 0 ||
        !g_command_processor)
        return;
    flush_pending_sync_locked();
}

void maybe_mark_vi_fb_dirty(uint32_t address, uint32_t length) {
    if (length == 0 || g_cpu_fb_dirty.load(std::memory_order_relaxed))
        return;

    const auto &vi = g_vi();
    const uint32_t origin = vi.reg_origin & 0xFFFFFFu;
    if (origin == 0 || origin == 0x280)
        return;

    const uint32_t type = vi.reg_status & 3u;
    // 0 = blank, 1 = reserved, 2 = RGBA5551, 3 = RGBA8888
    if (type < 2)
        return;

    const uint32_t width = vi.reg_width & 0xFFFu;
    if (width == 0)
        return;

    const uint32_t bytes_per_pixel = (type == 3) ? 4u : 2u;
    // Conservative NTSC bound; only used to detect CPU soft-FB traffic.
    const uint32_t fb_bytes = width * 480u * bytes_per_pixel;
    const uint32_t fb_end = origin + fb_bytes;
    if (address < fb_end && (address + length) > origin)
        g_cpu_fb_dirty.store(true, std::memory_order_relaxed);
}

void on_rdram_write(uint32_t address, uint32_t length) {
    maybe_mark_vi_fb_dirty(address, length);
    check_framebuffers(address, length);
}

void set_sync_full_callback(SyncFullCallback cb, void *userdata) {
    std::lock_guard lock(mutex());
    if (!g_command_processor)
        return;
    g_command_processor->set_sync_full_callback(cb, userdata);
}

bool is_dirty() {
    if (g_cpu_fb_dirty.load(std::memory_order_relaxed))
        return true;
    std::lock_guard lock(mutex());
    return g_rdp_dirty;
}

void clear_dirty() {
    std::lock_guard lock(mutex());
    g_rdp_dirty = false;
    g_cpu_fb_dirty.store(false, std::memory_order_relaxed);
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
