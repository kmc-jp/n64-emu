#include "cpu/cpu.h"
#include <cstdint>

namespace Utils {

uint32_t read_from_byte_array32(uint8_t *ptr) {
    return (ptr[0] << 24) + (ptr[1] << 16) + (ptr[2] << 8) + ptr[3];
}

void core_dump() { N64::n64cpu.dump(); }

} // namespace Utils