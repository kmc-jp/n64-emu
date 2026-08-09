#ifndef N64_SYSTEM
#define N64_SYSTEM

#include <cstdint>

namespace N64::Mmio::VI {
class VI;
}
namespace N64::N64System {
struct Config;
}

namespace N64 {
namespace N64System {

enum class N64Renderer {
    PARALLEL_RDP,
    CPU_RENDERER,
};

constexpr N64Renderer n64_renderer = N64Renderer::CPU_RENDERER;

void set_up(Config &config);
void shutdown();

using FieldPresentFn = void (*)(N64::Mmio::VI::VI &vi);
void set_field_present(FieldPresentFn fn);

struct PresentCounters {
    uint64_t presented = 0;
    uint64_t skipped = 0;
};
using PresentStatsFn = PresentCounters (*)();
void set_present_stats_fn(PresentStatsFn fn);

void step(Config &config);

} // namespace N64System
} // namespace N64

#endif
