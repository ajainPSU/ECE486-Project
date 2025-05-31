/*
* Alex Jain & Brandon Duong - 05/25/2025
* ECE 486 / Memory Trace Reader
* This program takes the translated binary from trace_reader.c
* and decodes them into the project specified instruction set.
*
* Version 1.2 - Integrated trace_reader.h and trace_reader.c for functionality.
*/

#include <stdio.h>
#include <stdint.h>
#include "trace_reader.h"
#include "functional_sim.h"

/*
// Instruction enums and structures.
typedef enum {
    ADD = 0x00, ADDI = 0x01,
    SUB = 0x02, SUBI = 0x03,
    MUL = 0x04, MULI = 0x05,
    OR  = 0x06, ORI  = 0x07,
    AND = 0x08, ANDI = 0x09,
    XOR = 0x0A, XORI = 0x0B,
    LDW = 0x0C, STW  = 0x0D,
    BZ  = 0x0E, BEQ  = 0x0F,
    JR  = 0x10, HALT = 0x11
} Opcode;

// Enum for instruction types.
typedef enum { R_TYPE, I_TYPE, INVALID_TYPE } InstrType;

typedef struct {
    Opcode opcode;
    InstrType type;
    int rs;
    int rt;
    int rd;         // Only used for R-type instructions.
    int immediate;  // Only used for I-type instructions.
} DecodedInstruction; */

// Function to get instruction type based on opcode.
InstrType get_instruction_type(uint8_t opcode) {
    switch (opcode) {
        case ADD: case SUB: case MUL:
        case OR: case AND: case XOR:
            return R_TYPE;
        case ADDI: case SUBI: case MULI:
        case ORI: case ANDI: case XORI:
        case LDW: case STW:
        case BZ: case BEQ: case JR: case HALT:
            return I_TYPE;
        default:
            return INVALID_TYPE;
    }
}

// Function to decode a binary instruction.
DecodedInstruction decode_instruction(uint32_t instr) {
    DecodedInstruction di;
    di.opcode = (instr >> 26) & 0x3F;
    di.type = get_instruction_type(di.opcode);

    switch (di.type) {
        case R_TYPE:
            di.rs = (instr >> 21) & 0x1F;
            di.rt = (instr >> 16) & 0x1F;
            di.rd = (instr >> 11) & 0x1F;
            di.immediate = 0;
            break;
        case I_TYPE:
            di.rs = (instr >> 21) & 0x1F;
            di.rt = (instr >> 16) & 0x1F;
            di.rd = 0;
            di.immediate = (int16_t)(instr & 0xFFFF); // Sign-extend
            break;
        default:
            di.rs = di.rt = di.rd = di.immediate = -1;
            break;
    }

    return di;
}

// Function to convert opcode to string.
const char* opcode_to_string(Opcode op) {
    switch (op) {
        case ADD: return "ADD"; case ADDI: return "ADDI";
        case SUB: return "SUB"; case SUBI: return "SUBI";
        case MUL: return "MUL"; case MULI: return "MULI";
        case OR:  return "OR";  case ORI:  return "ORI";
        case AND: return "AND"; case ANDI: return "ANDI";
        case XOR: return "XOR"; case XORI: return "XORI";
        case LDW: return "LDW"; case STW:  return "STW";
        case BZ:  return "BZ";  case BEQ:  return "BEQ";
        case JR:  return "JR";  case HALT: return "HALT";
        default:  return "UNKNOWN";
    }
}

// Function that trace_reader will call for each instruction
void process_binary(unsigned int value) {
    DecodedInstruction decoded = decode_instruction(value);

    // printf("Instruction: 0x%08X\tOpcode: %s\n", value, opcode_to_string(decoded.opcode));

    simulate_instruction(decoded);
}

/*
// Main program - Commented out for simulator
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory_image_file>\n", argv[0]);
        return 1;
    }

    read_memory_image(argv[1], process_binary);

    return 0;
} */
