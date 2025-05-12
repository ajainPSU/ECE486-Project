#ifndef INSTRUCTION_DECODER0_1_H
#define INSTRUCTION_DECODER0_1_H

#include <stdint.h>

//------------------------------------------------------------------------------
// Opcode enumeration (must match definitions in InstructionDecoder0.1.c)
//------------------------------------------------------------------------------
typedef enum {
    ADD, SUB, MUL, OR, AND, XOR,
    ADDI, SUBI, MULI, ORI, ANDI, XORI,
    LDW, STW,
    BZ, BEQ,
    JR, HALT,
    // Add any additional opcodes here
} Opcode;

//------------------------------------------------------------------------------
// Instruction format types
//------------------------------------------------------------------------------
typedef enum {
    R_TYPE,
    I_TYPE,
    J_TYPE  // if jump/branch instructions use a distinct format
} InstType;

//------------------------------------------------------------------------------
// DecodedInstruction: result of decoding a 32-bit instruction word
//------------------------------------------------------------------------------
typedef struct {
    Opcode    opcode;    // decoded opcode
    InstType  type;      // instruction format
    int        rs;       // first source register
    int        rt;       // second source register or destination for I-type
    int        rd;       // destination register for R-type
    int32_t    imm;      // sign-extended immediate (if applicable)
    uint32_t   raw;      // original 32-bit instruction word
} DecodedInstruction;

//------------------------------------------------------------------------------
// Decode a 32-bit MIPS-lite word into its fields
//------------------------------------------------------------------------------
DecodedInstruction decode_instruction(uint32_t word);

#endif // INSTRUCTION_DECODER0_1_H
