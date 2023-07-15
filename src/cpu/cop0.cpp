#include "cop0.h"

namespace N64 {
namespace Cpu {

// TODO: read, writeそれぞれで有効なreg_numかチェック

uint32_t Cop0::read32(uint32_t reg_num) const {
    assert(reg_num < 32);
    return static_cast<uint32_t>(reg[reg_num]);
}

uint64_t Cop0::read64(uint32_t reg_num) const {
    assert(reg_num < 32);
    return reg[reg_num];
}

void Cop0::write32(uint32_t reg_num, uint32_t value) {
    assert(reg_num < 32);
    reg[reg_num] = value;
}

void Cop0::write64(uint32_t reg_num, uint64_t value) {
    assert(reg_num < 32);
    reg[reg_num] = value;
}

} // namespace Cpu
} // namespace N64