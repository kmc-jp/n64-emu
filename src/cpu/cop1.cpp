#include "cpu/cop1.h"
#include "utils/log.h"

namespace N64 {
namespace Cpu {

void Cop1::dump() {
    // TODO: Implement
}

void Cop1::reset() {
    Utils::debug("Resetting CPU COP1");
    fcr0 = 0;
    fcr31.raw = 0;
    for (auto &reg : fgr) {
        reg.raw = 0;
    }
}

} // namespace Cpu
} // namespace N64
