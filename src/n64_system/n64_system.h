#ifndef N64_SYSTEM
#define N64_SYSTEM

#include "config.h"

namespace N64 {
namespace N64System {

void reset_all(Config& config);

void run(Config& config);

void step(Config &config);

} // namespace N64System
} // namespace N64

#endif