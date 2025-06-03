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
        case NOP: // NOP is typically an I-type or special type, but for simplicity, can be I_TYPE with immediate 0
            return I_TYPE;
        default:
            return INVALID_TYPE;
    }
}

DecodedInstruction decode_instruction(uint32_t instr) {
    DecodedInstruction decoded;
    decoded.opcode = (Opcode)((instr >> 26) & 0x3F); // Extract opcode
    decoded.type = get_instruction_type(decoded.opcode);

    // Initialize all fields to 0 to avoid garbage values for unused fields
    decoded.rs = 0;
    decoded.rt = 0;
    decoded.rd = 0;
    decoded.immediate = 0;

    switch (decoded.type) {
        case R_TYPE:
            decoded.rs = (instr >> 21) & 0x1F; // Extract rs
            decoded.rt = (instr >> 16) & 0x1F; // Extract rt
            decoded.rd = (instr >> 11) & 0x1F; // Extract rd
            break;
        case I_TYPE:
            decoded.rs = (instr >> 21) & 0x1F; // Extract rs
            decoded.rt = (instr >> 16) & 0x1F; // Extract rt (destination for ADDI/LDW, source for STW, branch target for BEQ/BZ)
            decoded.immediate = (int16_t)(instr & 0xFFFF); // Sign-extend immediate
            break;
        case INVALID_TYPE:
            // Handle invalid instruction type, perhaps by setting NOP or error
            decoded.opcode = NOP;
            decoded.type = I_TYPE;
            break;
    }

    return decoded;
}

/*
// Function to decode a binary instruction. (Last edited 05/31/2025 10:43PM)
DecodedInstruction decode_instruction(uint32_t instr) {
    DecodedInstruction di;

    // Added for NOP instruction.
    if (instr == 0x00000000) {
        di.opcode = NOP;
        di.type = R_TYPE; // or a new type NOP_TYPE if you'd like
        di.rs = di.rt = di.rd = 0;
        di.immediate = 0;
        return di;
    }

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
} */

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
        case NOP: return "NOP"; // NOP for pipelining.
        default:  return "UNKNOWN";
    }
}

// Function that trace_reader will call for each instruction
void process_binary(unsigned int value) {
    DecodedInstruction decoded = decode_instruction(value);

    // printf("Instruction: 0x%08X\tOpcode: %s\n", value, opcode_to_string(decoded.opcode));

    simulate_instruction(decoded);
}
