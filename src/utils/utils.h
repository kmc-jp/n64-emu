#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <source_location>

// PACK
#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__)                                                  \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

namespace Utils {

/* 指定されたポインタから4byte分を読み込む (big endian) */
uint32_t read_from_byte_array32(uint8_t *ptr);

/* 指定されたポインタに4byte分を書き込む (big endian) */
void write_to_byte_array32(uint8_t *ptr, uint32_t value);

void core_dump();

void unimplemented(const std::string what, const std::source_location loc = std::source_location::current());

} // namespace Utils

#endif