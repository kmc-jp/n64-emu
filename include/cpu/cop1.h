#ifndef COP1_H
#define COP1_H

#include "utils/pack.h"
#include <array>
#include <cstdint>

namespace N64 {
namespace Cpu {

union fcr31_t {
    PACK(struct {
        unsigned rounding_mode : 2;
        unsigned flag_inexact_operation : 1;
        unsigned flag_underflow : 1;
        unsigned flag_overflow : 1;
        unsigned flag_division_by_zero : 1;
        unsigned flag_invalid_operation : 1;
        unsigned enable_inexact_operation : 1;
        unsigned enable_underflow : 1;
        unsigned enable_overflow : 1;
        unsigned enable_division_by_zero : 1;
        unsigned enable_invalid_operation : 1;
        unsigned cause_inexact_operation : 1;
        unsigned cause_underflow : 1;
        unsigned cause_overflow : 1;
        unsigned cause_division_by_zero : 1;
        unsigned cause_invalid_operation : 1;
        unsigned cause_unimplemented_operation : 1;
        unsigned : 5;
        unsigned compare : 1;
        unsigned fs : 1;
        unsigned : 7;
    });

    PACK(struct {
        unsigned : 2;
        unsigned flag : 5;
        unsigned enable : 5;
        unsigned cause : 6;
        unsigned : 14;
    });

    uint32_t raw;
};

static_assert(sizeof(fcr31_t) == 4);

union fgr_t {
    PACK(struct {
        uint32_t lo;
        uint32_t hi;
    });
    uint64_t raw;
};

static_assert(sizeof(fgr_t) == 8);

// VR4300 FCR0: implementation 0x0B, revision 0x00
constexpr uint32_t FCR0_VR4300 = 0x00000B00;

class Cop1 {
  public:
    uint32_t fcr0;
    fcr31_t fcr31;
    std::array<fgr_t, 32> fgr;

    Cop1() {}

    void dump();

    void reset();

    // FR-aware FGR access (Status.FR). Bit transfers only; no float math.
    uint32_t get_fgr_word(uint8_t reg, bool fr) const;
    void set_fgr_word(uint8_t reg, uint32_t value, bool fr);
    uint64_t get_fgr_dword(uint8_t reg, bool fr) const;
    void set_fgr_dword(uint8_t reg, uint64_t value, bool fr);

    // Arithmetic S/W access: FR=0 forces even index and always uses .lo
    // (unlike MFC1/LWC1 which map odd regs to the even register's .hi).
    uint32_t get_fgr_word_arith(uint8_t reg, bool fr) const;
    void set_fgr_word_arith(uint8_t reg, uint32_t value, bool fr);

  private:
};

} // namespace Cpu
} // namespace N64

#endif
