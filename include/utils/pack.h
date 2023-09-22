#ifndef INCLUDE_GUARD_ED0640F1_2B5E_4A09_8485_04C925A21278
#define INCLUDE_GUARD_ED0640F1_2B5E_4A09_8485_04C925A21278

// PACK
#ifdef __GNUC__
#define PACK(Declaration) Declaration __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(Declaration)                                                      \
    __pragma(pack(push, 1)) Declaration __pragma(pack(pop))
#endif

#endif // INCLUDE_GUARD_ED0640F1_2B5E_4A09_8485_04C925A21278
