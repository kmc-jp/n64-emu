#ifndef GPR_H
#define GPR_H
#include <cassert>
#include <cstdint>

namespace N64 {
namespace Cpu{

class Gpr {
private:
    uint64_t gpr[32];

public:
    uint64_t read(uint32_t reg_num) const {
        assert(reg_num < 32);
        if (reg_num == 0) {
            return 0;
        } else {
            return gpr[reg_num];
        }
    }

    void write(uint32_t reg_num, uint64_t value) {
        assert(reg_num < 32);
        if (reg_num != 0) {
            gpr[reg_num] = value;
        }
    }
};
} // namespace Cpu
} // namespace N64

#endif