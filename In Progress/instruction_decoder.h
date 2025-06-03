#ifndef INSTRUCTION_DECODER_H
#define INSTRUCTION_DECODER_H

#include <stdint.h>

// Enum for opcodes (added NOP)
typedef enum {
    ADD = 0x00, ADDI = 0x01,
    SUB = 0x02, SUBI = 0x03,
    MUL = 0x04, MULI = 0x05,
    OR  = 0x06, ORI  = 0x07,
    AND = 0x08, ANDI = 0x09,
    XOR = 0x0A, XORI = 0x0B,
    LDW = 0x0C, STW  = 0x0D,
    BZ  = 0x0E, BEQ  = 0x0F,
    JR  = 0x10, HALT = 0x11,
    NOP = 0x12
} Opcode;


// Enum for instruction types
typedef enum { R_TYPE, I_TYPE, INVALID_TYPE } InstrType;

// Structure for decoded instructions
typedef struct {
    Opcode opcode;
    InstrType type;
    int rs;
    int rt;
    int rd;         // Only used for R-type instructions
    int immediate;  // Only used for I-type instructions
} DecodedInstruction;

// Function prototypes
InstrType get_instruction_type(uint8_t opcode);
DecodedInstruction decode_instruction(uint32_t instr);
const char* opcode_to_string(Opcode op);
void process_binary(unsigned int value);

#endif // INSTRUCTION_DECODER_H
