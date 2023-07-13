﻿#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "utils/utils.h"

namespace N64 {
namespace Cpu {

// rawはbig endianなので、逆順(opが最後)になる.
// 命令形式は以下のURLを参照
// https://hack64.net/docs/VR43XX.pdf
typedef union {
    uint32_t raw;
    // 1.4.3 CPU Instruction Set Overview
    // https://hack64.net/docs/VR43XX.pdf
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
        unsigned should_be_zero : 11;
        unsigned rd : 5;
        unsigned rt : 5;
        unsigned sub : 5;
        unsigned op : 6;
    } copz_type1;
} instruction_t;

// MIPS instructions
constexpr uint8_t OPCODE_SPECIAL = 0b000000;
constexpr uint8_t OPCODE_COP0 = 0b010000;
constexpr uint8_t OPCODE_ADDIU = 0b001001;
constexpr uint8_t OPCODE_LW = 0b100011;
constexpr uint8_t OPCODE_SW = 0b101011;
constexpr uint8_t OPCODE_LUI = 0b001111;
constexpr uint8_t OPCODE_BNE = 0b000101;
constexpr uint8_t OPCODE_BNEL = 0b010101;
constexpr uint8_t OPCODE_CACHE = 0b101111;
constexpr uint8_t OPCODE_ANDI = 0b001100;
constexpr uint8_t OPCODE_ORI = 0b001101;
constexpr uint8_t OPCODE_XORI = 0b001110;

constexpr uint8_t SPECIAL_FUNCT_SLL = 0b000000;  // SLL
constexpr uint8_t SPECIAL_FUNCT_SLTU = 0b101011; // SLTU
constexpr uint8_t SPECIAL_FUNCT_AND = 0b100100;  // AND
constexpr uint8_t SPECIAL_FUNCT_OR = 0b100101;   // OR
constexpr uint8_t SPECIAL_FUNCT_XOR = 0b100110;  // XOR
constexpr uint8_t SPECIAL_FUNCT_JR = 0b001000;   // JR

constexpr uint8_t CP0_SUB_MF = 0b00000;  // MFC0
constexpr uint8_t CP0_SUB_DMF = 0b00001; // DMFC0
constexpr uint8_t CP0_SUB_MT = 0b00100;  // MTC0
constexpr uint8_t CP0_SUB_DMT = 0b00101; // DMT

} // namespace Cpu
} // namespace N64

#endif