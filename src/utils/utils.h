// PACK
#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__)                                                  \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

// Sign extension
#define SEXT32_64(x) (int64_t)((int32_t)(x))
