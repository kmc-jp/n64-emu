#include "cpu/instruction.h"

namespace N64 {
namespace Cpu {

void assert_encoding_is_valid(bool validity, const std::source_location loc) {
    // should be able to ignore?
    if (!validity) {
        Utils::critical("Assertion failed at {}, line {}", loc.file_name(),
                        loc.line());
        exit(-1);
    }
}
} // namespace Cpu
} // namespace N64
