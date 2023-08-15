#ifndef N64_SYSTEM
#define N64_SYSTEM

#include "config.h"
#include "app/parallel_rdp_wrapper.h"

namespace N64 {
namespace N64System {

void set_up(Config &config);

void step(Config &config, Vulkan::WSI& wsi);

} // namespace N64System
} // namespace N64

#endif