#include "cpu/cpu.h"
#include <cstdint>
#include <source_location>
#include <spdlog/spdlog.h>

namespace Utils {

uint32_t read_from_byte_array32(uint8_t *ptr) {
    return (ptr[0] << 24) + (ptr[1] << 16) + (ptr[2] << 8) + ptr[3];
}

void write_to_byte_array32(uint8_t *ptr, uint32_t value) {
    uint8_t b1 = value & 0xFF;
    uint8_t b2 = (value & 0xFF00) >> 8;
    uint8_t b3 = (value & 0xFF0000) >> 16;
    uint8_t b4 = (value & 0xFF000000) >> 24;
    ptr[0] = b4;
    ptr[1] = b3;
    ptr[2] = b2;
    ptr[3] = b1;
}

void core_dump() { N64::g_cpu().dump(); }

void unimplemented(const std::string what, const std::source_location loc) {
    spdlog::critical("Unimplemented. {}", what);
    spdlog::critical("In `{}` at {}:({},{})",
                     loc.function_name(), loc.file_name(), loc.line(),
                     loc.column());
    core_dump();
    exit(-1);
}

} // namespace Utils