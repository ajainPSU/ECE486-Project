// fwd.h
#ifndef FWD_H
#define FWD_H

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// OPCODE definitions (6-bit) for MIPS-lite (from Spec, Section 1.1–1.2)
#define OPCODE_ADD   0x00
#define OPCODE_ADDI  0x01
#define OPCODE_SUB   0x02
#define OPCODE_SUBI  0x03
#define OPCODE_MUL   0x04
#define OPCODE_MULI  0x05
#define OPCODE_OR    0x06
#define OPCODE_ORI   0x07
#define OPCODE_AND   0x08
#define OPCODE_ANDI  0x09
#define OPCODE_XOR   0x0A
#define OPCODE_XORI  0x0B
#define OPCODE_LDW   0x0C
#define OPCODE_STW   0x0D
#define OPCODE_BZ    0x0E
#define OPCODE_BEQ   0x0F
#define OPCODE_JR    0x10
#define OPCODE_HALT  0x11

// Sentinel for NOP/invalid opcode:
#define NOP_OPCODE   0x3F

// Shared constants
#define MEM_SIZE_WORDS 1024

// -----------------------------------------------------------------------------
// “Decoded instruction” structure
typedef struct {
    uint8_t  opcode;    // 6-bit opcode
    uint8_t  rs;        // source register #1 (0–31)
    uint8_t  rt;        // source/reg-dest (I-type) or source2 (R-type)
    uint8_t  rd;        // dest register for R-type (0–31)
    int32_t  imm;       // sign-extended 16-bit immediate (I-type)
    bool     valid;     // false → this is a NOP
} Instr;

// -----------------------------------------------------------------------------
// Pipeline latch registers (IF/ID, ID/EX, EX/MEM, MEM/WB)
typedef struct {
    Instr    instr;
    int32_t  reg_val_rs;    // value read from register file (for ID/EX)
    int32_t  reg_val_rt;    // value read from register file or STW data (for ID/EX)
    int32_t  alu_result;    // result computed in EX stage (for EX/MEM)
    int32_t  mem_data;      // data loaded in MEM stage (for MEM/WB)
} IFID_Reg, IDEX_Reg, EXMEM_Reg, MEMWB_Reg;

// -----------------------------------------------------------------------------
// Top-level simulator entry points (called from main)
void simulate_no_forwarding(const char *mem_image_path);
void simulate_with_forwarding(const char *mem_image_path);

#endif // FWD_H
