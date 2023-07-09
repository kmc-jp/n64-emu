#ifndef UTILS_H
#define UTILS_H

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

} // namespace Utils

#endif