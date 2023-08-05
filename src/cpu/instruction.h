#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "utils/utils.h"
#include <cstdint>

namespace N64 {
namespace Cpu {

// rawはbig endianなので、逆順(opが最後)になる.
// 命令形式は以下のURLを参照
// https://hack64.net/docs/VR43XX.pdf
// NOTE: reverse order when using big endian machine!
typedef union {
    uint32_t raw;
    // 1.4.3 CPU Instruction Set Overview
    // https://hack64.net/docs/VR43XX.pdf
    PACK(struct {
        unsigned : 26;
        unsigned op : 6;
    });

    PACK(struct {
        unsigned imm : 16;
        unsigned rt : 5;
        unsigned rs : 5;
        unsigned op : 6;
    })
    i_type;

    PACK(struct {
        unsigned target : 26;
        unsigned op : 6;
    })
    j_type;

    PACK(struct {
        unsigned funct : 6;
        unsigned sa : 5;
        unsigned rd : 5;
        unsigned rt : 5;
        unsigned rs : 5;
        unsigned op : 6;
    })
    r_type;

    PACK(struct {
        unsigned should_be_zero : 11;
        unsigned rd : 5;
        unsigned rt : 5;
        unsigned sub : 5;
        unsigned op : 6;
    })
    copz_type1;

    PACK(struct {
        unsigned offset : 16;
        unsigned ft : 5;
        unsigned base : 5;
        unsigned op : 6;
    })
    fi_type;
} instruction_t;

static_assert(sizeof(instruction_t) == 4, "instruction_t size is not 4");

// MIPS instructions
constexpr uint8_t OPCODE_SPECIAL = 0b000000;
constexpr uint8_t OPCODE_REGIMM = 0b000001;
constexpr uint8_t OPCODE_CP0 = 0b010000;
constexpr uint8_t OPCODE_CP1 = 0b010001;

constexpr uint8_t OPCODE_J = 0b000010;
constexpr uint8_t OPCODE_JAL = 0b000011;
constexpr uint8_t OPCODE_ADDI = 0b001000;
constexpr uint8_t OPCODE_ADDIU = 0b001001;
constexpr uint8_t OPCODE_DADDI = 0b011000;
constexpr uint8_t OPCODE_LW = 0b100011;
constexpr uint8_t OPCODE_LWU = 0b100111;
constexpr uint8_t OPCODE_LUI = 0b001111;
constexpr uint8_t OPCODE_LHU = 0b100101;
constexpr uint8_t OPCODE_LD = 0b110111;
constexpr uint8_t OPCODE_SW = 0b101011;
constexpr uint8_t OPCODE_SD = 0b111111;
constexpr uint8_t OPCODE_LDL = 0b011010;
constexpr uint8_t OPCODE_LDR = 0b011011;
constexpr uint8_t OPCODE_SDL = 0b101100;
constexpr uint8_t OPCODE_SDR = 0b101101;

constexpr uint8_t OPCODE_BEQ = 0b000100;
constexpr uint8_t OPCODE_BEQL = 0b010100;
constexpr uint8_t OPCODE_BNE = 0b000101;
constexpr uint8_t OPCODE_BNEL = 0b010101;

constexpr uint8_t OPCODE_BLEZ = 0b000110;
constexpr uint8_t OPCODE_BLEZL = 0b010110;
constexpr uint8_t OPCODE_BGTZ = 0b000111;
constexpr uint8_t OPCODE_BGTZL = 0b010111;

constexpr uint8_t OPCODE_CACHE = 0b101111;
constexpr uint8_t OPCODE_ANDI = 0b001100;
constexpr uint8_t OPCODE_ORI = 0b001101;
constexpr uint8_t OPCODE_XORI = 0b001110;

constexpr uint8_t OPCODE_SLTI = 0b001010;
constexpr uint8_t OPCODE_SLTIU = 0b001011;

constexpr uint8_t SPECIAL_FUNCT_ADD = 0b100000;   // ADD
constexpr uint8_t SPECIAL_FUNCT_ADDU = 0b100001;  // ADDU
constexpr uint8_t SPECIAL_FUNCT_SUB = 0b100010;   // SUB
constexpr uint8_t SPECIAL_FUNCT_SUBU = 0b100011;  // SUBU
constexpr uint8_t SPECIAL_FUNCT_MULT = 0b011000;  // MULT
constexpr uint8_t SPECIAL_FUNCT_MULTU = 0b011001; // MULTU
constexpr uint8_t SPECIAL_FUNCT_DIV = 0b011010;   // DIV
constexpr uint8_t SPECIAL_FUNCT_DIVU = 0b011011;  // DIVU
constexpr uint8_t SPECIAL_FUNCT_SLL = 0b000000;   // SLL
constexpr uint8_t SPECIAL_FUNCT_SRL = 0b000010;   // SRL
constexpr uint8_t SPECIAL_FUNCT_SRA = 0b000011;   // SRA
constexpr uint8_t SPECIAL_FUNCT_SRAV = 0b000111;  // SRAV
constexpr uint8_t SPECIAL_FUNCT_SLLV = 0b000100;  // SLLV
constexpr uint8_t SPECIAL_FUNCT_SRLV = 0b000110;  // SRLV
constexpr uint8_t SPECIAL_FUNCT_SLT = 0b101010;   // SLT
constexpr uint8_t SPECIAL_FUNCT_SLTU = 0b101011;  // SLTU
constexpr uint8_t SPECIAL_FUNCT_AND = 0b100100;   // AND
constexpr uint8_t SPECIAL_FUNCT_OR = 0b100101;    // OR
constexpr uint8_t SPECIAL_FUNCT_XOR = 0b100110;   // XOR
constexpr uint8_t SPECIAL_FUNCT_JR = 0b001000;    // JR
constexpr uint8_t SPECIAL_FUNCT_JALR = 0b001001;  // JALR
constexpr uint8_t SPECIAL_FUNCT_MFHI = 0b010000;  // MFHI
constexpr uint8_t SPECIAL_FUNCT_MFLO = 0b010010;  // MFLO
constexpr uint8_t SPECIAL_FUNCT_DSLL = 0b111000;
constexpr uint8_t SPECIAL_FUNCT_DSRL = 0b111010;
constexpr uint8_t SPECIAL_FUNCT_DSRA = 0b111011;
constexpr uint8_t SPECIAL_FUNCT_DSLL32 = 0b111100;
constexpr uint8_t SPECIAL_FUNCT_DSRL32 = 0b111110;
constexpr uint8_t SPECIAL_FUNCT_DSRA32 = 0b111111;

// Regimm
constexpr uint8_t REGIMM_RT_BLTZ = 0b00000;   // BLTZ
constexpr uint8_t REGIMM_RT_BLTZL = 0b00010;  // BLTZL
constexpr uint8_t REGIMM_RT_BGEZ = 0b00001;   // BGEZ
constexpr uint8_t REGIMM_RT_BGEZL = 0b00011;  // BGEZL
constexpr uint8_t REGIMM_RT_BLTZAL = 0b10000; // BLTZAL
constexpr uint8_t REGIMM_RT_BGEZAL = 0b10001; // BGEZAL

// COP
constexpr uint8_t COP_MFC = 0b00000;  // MFC
constexpr uint8_t COP_DMFC = 0b00001; // DMFC
constexpr uint8_t COP_MTC = 0b00100;  // MTC
constexpr uint8_t COP_DMTC = 0b00101; // DMT
constexpr uint8_t COP_CFC = 0b00010;  // CFC
constexpr uint8_t COP_CTC = 0b00110;  // CTC

static void assert_encoding_is_valid(bool validity) {
    // should be able to ignore?
    assert(validity);
}

} // namespace Cpu
} // namespace N64

#endif