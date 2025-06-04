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
#include "functional_sim.h" // Needed for simulate_instruction and global state
#include "instruction_decoder.h"

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
        case NOP:
            return I_TYPE;
        default:
            return INVALID_TYPE;
    }
}

// Function to decode a binary instruction.
DecodedInstruction decode_instruction(uint32_t instr) {
    DecodedInstruction decoded;
    decoded.opcode = (Opcode)((instr >> 26) & 0x3F); // Extract opcode (bits 31-26)
    decoded.type = get_instruction_type(decoded.opcode);

    // Initialize all fields to 0 to avoid garbage values for unused fields
    decoded.rs = 0;
    decoded.rt = 0;
    decoded.rd = 0;
    decoded.immediate = 0;

    switch (decoded.type) {
        case R_TYPE:
            decoded.rs = (instr >> 21) & 0x1F; // Extract rs (bits 25-21)
            decoded.rt = (instr >> 16) & 0x1F; // Extract rt (bits 20-16)
            decoded.rd = (instr >> 11) & 0x1F; // Extract rd (bits 15-11)
            break;
        case I_TYPE:
            decoded.rs = (instr >> 21) & 0x1F; // Extract rs (bits 25-21)
            decoded.rt = (instr >> 16) & 0x1F; // Extract rt (bits 20-16)
            decoded.immediate = (int16_t)(instr & 0xFFFF); // Extract and sign-extend immediate (bits 15-0)
            break;
        case INVALID_TYPE:
            decoded.opcode = NOP; // Fallback to NOP for invalid instructions
            decoded.type = I_TYPE; // NOP is handled as I_TYPE for simplicity
            break;
    }
    return decoded;
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
        case NOP: return "NOP";
        default:  return "UNKNOWN";
    }
}

// This function is for the functional simulator's direct execution.
// It's called by trace_reader for FS mode, or by functional_sim.c directly.
void process_binary(unsigned int value) {
    DecodedInstruction decoded = decode_instruction(value);
    simulate_instruction(decoded);
}