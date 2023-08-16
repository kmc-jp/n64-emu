#pragma once

#include <mmio/vi.h>
#include <rdp_device.hpp>
#include <utils/utils.h>
#include <wsi.hpp>

namespace N64 {
namespace PRDPWrapper {

void init_prdp(Vulkan::WSI &wsi, uint8_t* rdram);

void fini_prdp();

void update_screen(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi);

} // namespace PRDPWrapper
} // namespace N64
