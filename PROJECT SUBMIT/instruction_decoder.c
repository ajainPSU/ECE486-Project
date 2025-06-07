/*
* Instruction Decoder
* This file contains the implementation of the instruction decoder for MIPS-lite instructions.
* It decodes binary instructions into a structured format and provides utility functions.
* The decoder supports R-type and I-type instructions, extracting relevant fields such as opcode,
* source registers, destination registers, and immediate values.
* It also includes a function to convert opcodes to their string representations.
*
* Supported Operations:
* - R-type instructions: ADD, SUB, MUL, OR, AND, XOR
* - I-type instructions: ADDI, SUBI, MULI, ORI, ANDI, XORI, LDW, STW, BZ, BEQ, JR, HALT, NOP
*
* Functions:
* - get_instruction_type: Determines the type of instruction based on the opcode.
* - decode_instruction: Decodes a binary instruction into a structured format.
* - opcode_to_string: Converts an opcode to its string representation.
*
*/

#include <stdio.h>
#include <stdint.h>
#include "functional_sim.h" // Needed for simulate_instruction and global state
#include "instruction_decoder.h"

/*
* Instruction Types
* R_TYPE: Register type instructions that operate on registers.
* I_TYPE: Immediate type instructions that include immediate values.
* INVALID_TYPE: Used for unrecognized or invalid instructions.
*/
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

/*
* DecodedInstruction Structure
* This structure holds the decoded instruction fields.
* It includes the opcode, instruction type, source registers (rs, rt), destination register (rd),
* and immediate value (for I-type instructions).
*/
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
            decoded.rd = 0; // rd is not used in I_TYPE instructions
            break;
        case INVALID_TYPE:
            fprintf(stderr, "Warning: Instruction with UNKNOWN/INVALID_TYPE for decoded opcode 0x%02X (raw instr: 0x%08X)\n", (unsigned int)decoded.opcode, instr);
            break;
        default: // Also catches if decoded.type wasn't set.
            fprintf(stderr, "Error: Invalid instruction type for opcode 0x%02X (raw instr: 0x%08X)\n", (unsigned int)decoded.opcode, instr);
            // Set NOP as fallback.
            decoded.opcode = NOP;
            decoded.type = I_TYPE;
            decoded.rs = 0;
            decoded.rt = 0;
            decoded.rd = 0;
            decoded.immediate = 0;
            break;
    }
    return decoded;
}

/*
* Opcode to String Conversion
* This function converts an opcode to its string representation.
* It is useful for debugging and logging purposes.
* It returns a string literal corresponding to the opcode.
* If the opcode is unknown, it returns "UNKNOWN".
*/
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

/*
* Process Binary Instruction
* This function takes a binary instruction value, decodes it,
* and simulates the instruction using the global state.
* It is the entry point for processing binary instructions in the functional simulator.
*/
void process_binary(unsigned int value) {
    DecodedInstruction decoded = decode_instruction(value);
    simulate_instruction(decoded);
}
