#pragma once

#include <cstdint>

namespace Vulkan {
class WSI;
}
namespace N64::Mmio::VI {
class VI;
}

namespace N64 {
namespace PRDPWrapper {

void init_prdp(Vulkan::WSI &wsi, uint8_t *rdram);

void fini_prdp();

void update_screen(Vulkan::WSI &wsi, N64::Mmio::VI::VI &vi);

} // namespace PRDPWrapper
} // namespace N64
