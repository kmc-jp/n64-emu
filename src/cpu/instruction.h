#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "../utils/utils.h"

namespace N64 {
namespace Cpu {

// rawはbig endianなので、逆順(opが最後)になる.
// ref: Chap 3.1 https://hack64.net/docs/VR43XX.pdf
typedef union {
    uint32_t raw;

    struct {
        unsigned : 26;
        unsigned op : 6;
    };

    struct {
        unsigned imm : 16;
        unsigned rt : 5;
        unsigned rs : 5;
        unsigned op : 6;
    } i_type;

    struct {
        unsigned target : 26;
        unsigned op : 6;
    } j_type;

    struct {
        unsigned funct : 6;
        unsigned sa : 5;
        unsigned rd : 5;
        unsigned rt : 5;
        unsigned rs : 5;
        unsigned op : 6;
    } r_type;

    struct {
        unsigned should_be_zero : 6;
        unsigned sa : 5;
        unsigned rd : 5;
        unsigned sub : 5;
        unsigned op : 6;
    } copz_a;
} instruction_t;

const uint8_t mask5 = 0b11'111;
const uint8_t mask6 = 0b11'1111;
const uint16_t mask11 = 0b111'1111'1111;
const uint16_t mask16 = 0b1111'1111'1111'1111;
const uint8_t mask26 = 0x3FFFFFF;

// single instruction
// フォーマットは以下のURLを参照
// https://hack64.net/docs/VR43XX.pdf

class Instruction {
  public:
    uint32_t raw;

    uint8_t op;
    // CPU instructions

    // I-Type
    uint8_t rs;
    uint8_t rt;
    uint16_t imm;
    // J-Type
    uint32_t target;
    // R-Type
    // rs
    // rt
    uint8_t rd;
    uint8_t sa;
    uint8_t funct;

    // COP instructions
    // Chapter 3.2.5 https://hack64.net/docs/VR43XX.pdf

    // CP0 instructions
    // Chapter 3.2.6 https://hack64.net/docs/VR43XX.pdf
    uint8_t sub;
    uint16_t should_be_zero;

    // TODO: rest of COP instructions

    Instruction(uint32_t big_endian_raw) {
        raw = big_endian_raw;
        op = (raw >> 26) & mask6;

        // I-Type
        rs = (raw >> 21) & mask5;
        rt = (raw >> 16) & mask5;
        imm = raw & mask16;
        // J-Type
        target = raw & mask26;
        // R-Type
        rd = (raw >> 11) & mask5;
        sa = (raw >> 6) & mask5;
        funct = raw & mask6;

        // CP0
        sub = (raw >> 21) & mask5;
        // rt
        // rd
        should_be_zero = raw & mask11;

        // TODO: rest of COP instructions
    }
};


const uint8_t OPCODE_COP0 = 0b010000;
const uint8_t OPCODE_LUI = 0b001111;

const uint8_t CP0_SUB_MT = 0b00100;

} // namespace Cpu
} // namespace N64

#endif
