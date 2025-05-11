#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum { R_TYPE, I_TYPE, INVALID_TYPE } InstrType;

typedef struct {
    Opcode opcode;
    InstrType type;
    int rs;
    int rt;
    int rd;
    int immediate;
} DecodedInstruction;

InstrType get_instruction_type(uint8_t opcode) {
    switch (opcode) {
        case ADD: case SUB: case MUL:
        case OR: case AND: case XOR:
            return R_TYPE;
        case ADDI: case SUBI: 
        case MULI: case ORI: 
        case ANDI: case XORI:
        case LDW: case STW:
        case BZ: case BEQ: 
        case JR: case HALT:
            return I_TYPE;
        default:
            return INVALID_TYPE;
    }
}

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
            // sign-extend 16-bit immediate
            di.immediate = (int16_t)(instr & 0xFFFF);
            break;
        default:
            di.rs = di.rt = di.rd = di.immediate = -1;
            break;
    }

    return di;
}

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

// Sample test
int main() {
    uint32_t raw_instr = 0x00853000; // Example from spec (ADD R6, R4, R5)
    DecodedInstruction decoded = decode_instruction(raw_instr);

    printf("Decoded Instruction:\n");
    printf("Opcode: %s\n", opcode_to_string(decoded.opcode));
    if (decoded.type == R_TYPE) {
        printf("Type: R-type\n");
        printf("Rs: R%d, Rt: R%d, Rd: R%d\n", decoded.rs, decoded.rt, decoded.rd);
    } else if (decoded.type == I_TYPE) {
        printf("Type: I-type\n");
        printf("Rs: R%d, Rt: R%d, Imm: %d\n", decoded.rs, decoded.rt, decoded.immediate);
    } else {
        printf("Invalid instruction\n");
    }

    return 0;
}
