#pragma once

#include "memory/memory.h"
#include "mmio/vi.h"
#include "utils.h"
#include "vulkan_headers.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <algorithm>
#include <device.hpp>
#include <rdp_device.hpp>
#include <volk.h>
#include <wsi.hpp>

namespace N64 {
namespace Frontend {

init_vulkan_wsi(_wsiPlatform, std::make_unique<QtParallelRdpWindowInfo>(pane));

init_parallel_rdp();

void prdp_update_screen();

} // namespace Frontend
} // namespace N64
