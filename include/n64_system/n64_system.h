#ifndef N64_SYSTEM
#define N64_SYSTEM

namespace Vulkan {
class WSI;
}
namespace N64::N64System {
struct Config;
}

namespace N64 {
namespace N64System {

// TODO: unused now. should remove?
enum class N64Renderer {
    PARALLEL_RDP,
    CPU_RENDERER,
};

// TODO: unused now. should remove?
// TODO: support other renderer
constexpr N64Renderer n64_renderer = N64Renderer::CPU_RENDERER;

void set_up(Config &config);

void step(Config &config, Vulkan::WSI &wsi);

} // namespace N64System
} // namespace N64

#endif
