// fwd.c
//
// Implements two simulators for a 5-stage in-order MIPS-lite pipeline:
//   1) No-forwarding version
//   2) Full-forwarding version
//
// Each simulator:
//   - Reads a 4 KB memory image (1024 lines of 32-bit hex words).
//   - Executes all instructions until HALT.
//   - No-forwarding prints: total_cycles_no_forward.
//   - Forwarding prints: total_cycles_forward, hazards_not_eliminated_by_forwarding, speedup.
//
// Usage:
//   gcc -std=c11 -O2 fwd.c -o fwd
//   ./fwd <memory_image.txt>
//
// References:
//   • Opcode set & instruction format: Section 1.1–1.2 (Op codes) :contentReference[oaicite:0]{index=0}
//   • Pipeline behavior (5-stage, hazard rules, branch-flush): Section 1.5.i/1.5.ii 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fwd.h"

// -----------------------------------------------------------------------------
// Global memory + register file (reset between simulators)
static uint32_t Mem[MEM_SIZE_WORDS];  // 4 KB memory, word-addressable
static int32_t  RegFile[32];          // R0–R31 (R0 hardwired to 0)

// Statistics (populated by each simulator)
static uint64_t cycles_no_forward      = 0;
static uint64_t cycles_forward         = 0;
static uint64_t hazards_not_forwarded  = 0;

// -----------------------------------------------------------------------------
// Helper: sign-extend a 16-bit immediate to 32 bits
static inline int32_t sign_extend_16(uint16_t imm16) {
    if (imm16 & 0x8000) {
        return (int32_t)((int32_t)imm16 - (int32_t)0x10000);
    } else {
        return (int32_t)imm16;
    }
}

// -----------------------------------------------------------------------------
// Decode a 32-bit instruction word into an Instr struct
static Instr decode_instr(uint32_t word) {
    Instr I;
    memset(&I, 0, sizeof(I));
    uint8_t opcode = (word >> 26) & 0x3F;
    I.opcode = opcode;

    // If opcode is outside defined range, mark as NOP
    if (opcode > OPCODE_HALT) {
        I.valid = false;
        return I;
    }
    I.valid = true;

    // Extract fields
    I.rs  = (word >> 21) & 0x1F;
    I.rt  = (word >> 16) & 0x1F;
    I.rd  = (word >> 11) & 0x1F;
    I.imm = sign_extend_16((uint16_t)(word & 0xFFFF));
    return I;
}

// -----------------------------------------------------------------------------
// Execute a single instruction’s functional semantics.
//   • Updates RegFile[] or Mem[] accordingly.
//   • Returns the next PC (or 0xFFFFFFFF to indicate HALT).
static uint32_t execute_functional(Instr *I, uint32_t PC) {
    if (!I->valid) {
        // NOP: just advance
        return PC + 4;
    }

    switch (I->opcode) {
        // R-type arithmetic/logical
        case OPCODE_ADD:
            RegFile[I->rd] = RegFile[I->rs] + RegFile[I->rt];
            return PC + 4;
        case OPCODE_SUB:
            RegFile[I->rd] = RegFile[I->rs] - RegFile[I->rt];
            return PC + 4;
        case OPCODE_MUL:
            RegFile[I->rd] = RegFile[I->rs] * RegFile[I->rt];
            return PC + 4;
        case OPCODE_OR:
            RegFile[I->rd] = RegFile[I->rs] | RegFile[I->rt];
            return PC + 4;
        case OPCODE_AND:
            RegFile[I->rd] = RegFile[I->rs] & RegFile[I->rt];
            return PC + 4;
        case OPCODE_XOR:
            RegFile[I->rd] = RegFile[I->rs] ^ RegFile[I->rt];
            return PC + 4;

        // I-type arithmetic/logical
        case OPCODE_ADDI:
            RegFile[I->rt] = RegFile[I->rs] + I->imm;
            return PC + 4;
        case OPCODE_SUBI:
            RegFile[I->rt] = RegFile[I->rs] - I->imm;
            return PC + 4;
        case OPCODE_MULI:
            RegFile[I->rt] = RegFile[I->rs] * I->imm;
            return PC + 4;
        case OPCODE_ORI:
            RegFile[I->rt] = RegFile[I->rs] | (uint32_t)I->imm;
            return PC + 4;
        case OPCODE_ANDI:
            RegFile[I->rt] = RegFile[I->rs] & (uint32_t)I->imm;
            return PC + 4;
        case OPCODE_XORI:
            RegFile[I->rt] = RegFile[I->rs] ^ (uint32_t)I->imm;
            return PC + 4;

        // Memory access
        case OPCODE_LDW: {
            int32_t addr = RegFile[I->rs] + I->imm;
            if (addr < 0 || (addr % 4) || (addr / 4) >= MEM_SIZE_WORDS) {
                fprintf(stderr, "LDW address out of range at PC=0x%08X\n", PC);
                exit(1);
            }
            RegFile[I->rt] = (int32_t)Mem[addr / 4];
            return PC + 4;
        }
        case OPCODE_STW: {
            int32_t addr = RegFile[I->rs] + I->imm;
            if (addr < 0 || (addr % 4) || (addr / 4) >= MEM_SIZE_WORDS) {
                fprintf(stderr, "STW address out of range at PC=0x%08X\n", PC);
                exit(1);
            }
            Mem[addr / 4] = (uint32_t)RegFile[I->rt];
            return PC + 4;
        }

        // Branch: BZ, BEQ
        case OPCODE_BZ:
            if (RegFile[I->rs] == 0) {
                return PC + (I->imm * 4);
            } else {
                return PC + 4;
            }
        case OPCODE_BEQ:
            if (RegFile[I->rs] == RegFile[I->rt]) {
                return PC + (I->imm * 4);
            } else {
                return PC + 4;
            }

        // Jump Register
        case OPCODE_JR:
            return (uint32_t)RegFile[I->rs];

        // HALT
        case OPCODE_HALT:
            return 0xFFFFFFFF;

        default:
            // Should not happen
            return PC + 4;
    }
}

// -----------------------------------------------------------------------------
// Helper to load a 1024-line memory image (4 KB) from a text file
static void load_memory_image(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen");
        exit(1);
    }
    char line[64];
    for (int i = 0; i < MEM_SIZE_WORDS; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            fprintf(stderr, "Error: memory image must have 1024 lines (got %d)\n", i);
            fclose(fp);
            exit(1);
        }
        unsigned int val = 0;
        if (sscanf(line, "%x", &val) != 1) {
            fprintf(stderr, "Error: invalid hex on line %d: %s", i + 1, line);
            fclose(fp);
            exit(1);
        }
        Mem[i] = (uint32_t)val;
    }
    fclose(fp);
}

// -----------------------------------------------------------------------------
// “No-forwarding” 5-stage pipeline simulation.
// Populates cycles_no_forward, then prints it.
void simulate_no_forwarding(const char *mem_image_path) {
    // Load memory image
    load_memory_image(mem_image_path);

    // Initialize registers
    for (int i = 0; i < 32; i++) {
        RegFile[i] = 0;
    }

    // Pipeline latch initialization (all NOP)
    IFID_Reg  IF_ID  = { .instr.valid = false };
    IDEX_Reg  ID_EX  = { .instr.valid = false };
    EXMEM_Reg EX_MEM = { .instr.valid = false };
    MEMWB_Reg MEM_WB = { .instr.valid = false };

    uint32_t PC    = 0;
    bool     done  = false;
    cycles_no_forward = 0;

    while (!done) {
        cycles_no_forward++;

        // --------------------------
        // 1) WB stage
        if (MEM_WB.instr.valid) {
            Instr *W = &MEM_WB.instr;
            switch (W->opcode) {
                case OPCODE_ADD:
                case OPCODE_SUB:
                case OPCODE_MUL:
                case OPCODE_OR:
                case OPCODE_AND:
                case OPCODE_XOR:
                    RegFile[W->rd] = MEM_WB.alu_result;
                    break;
                case OPCODE_ADDI:
                case OPCODE_SUBI:
                case OPCODE_MULI:
                case OPCODE_ORI:
                case OPCODE_ANDI:
                case OPCODE_XORI:
                case OPCODE_LDW:
                    RegFile[W->rt] = MEM_WB.mem_data;
                    break;
                default:
                    break;
            }
        }

        // --------------------------
        // 2) MEM stage
        if (EX_MEM.instr.valid) {
            Instr *M = &EX_MEM.instr;
            MEM_WB = (MEMWB_Reg){ .instr = *M };
            switch (M->opcode) {
                case OPCODE_LDW: {
                    int32_t addr = EX_MEM.alu_result;
                    if (addr < 0 || (addr % 4) || (addr / 4) >= MEM_SIZE_WORDS) {
                        fprintf(stderr, "LDW address OOB in no-forward at cycle %llu\n",
                                cycles_no_forward);
                        exit(1);
                    }
                    MEM_WB.mem_data   = (int32_t)Mem[addr / 4];
                    MEM_WB.alu_result = 0; // not used
                    break;
                }
                case OPCODE_STW: {
                    int32_t addr = EX_MEM.alu_result;
                    if (addr < 0 || (addr % 4) || (addr / 4) >= MEM_SIZE_WORDS) {
                        fprintf(stderr, "STW address OOB in no-forward at cycle %llu\n",
                                cycles_no_forward);
                        exit(1);
                    }
                    Mem[addr / 4] = (uint32_t)EX_MEM.reg_val_rt;
                    MEM_WB.mem_data   = 0;
                    MEM_WB.alu_result = 0;
                    break;
                }
                default:
                    MEM_WB.alu_result = EX_MEM.alu_result;
                    MEM_WB.mem_data   = EX_MEM.alu_result;
                    break;
            }
        } else {
            MEM_WB.instr.valid = false;
        }

        // --------------------------
        // 3) EX stage
        if (ID_EX.instr.valid) {
            Instr *E = &ID_EX.instr;
            EX_MEM = (EXMEM_Reg){ .instr = *E, .reg_val_rt = ID_EX.reg_val_rt };
            switch (E->opcode) {
                // R-type
                case OPCODE_ADD:
                    EX_MEM.alu_result = ID_EX.reg_val_rs + ID_EX.reg_val_rt;
                    break;
                case OPCODE_SUB:
                    EX_MEM.alu_result = ID_EX.reg_val_rs - ID_EX.reg_val_rt;
                    break;
                case OPCODE_MUL:
                    EX_MEM.alu_result = ID_EX.reg_val_rs * ID_EX.reg_val_rt;
                    break;
                case OPCODE_OR:
                    EX_MEM.alu_result = ID_EX.reg_val_rs | ID_EX.reg_val_rt;
                    break;
                case OPCODE_AND:
                    EX_MEM.alu_result = ID_EX.reg_val_rs & ID_EX.reg_val_rt;
                    break;
                case OPCODE_XOR:
                    EX_MEM.alu_result = ID_EX.reg_val_rs ^ ID_EX.reg_val_rt;
                    break;

                // I-type
                case OPCODE_ADDI:
                    EX_MEM.alu_result = ID_EX.reg_val_rs + E->imm;
                    break;
                case OPCODE_SUBI:
                    EX_MEM.alu_result = ID_EX.reg_val_rs - E->imm;
                    break;
                case OPCODE_MULI:
                    EX_MEM.alu_result = ID_EX.reg_val_rs * E->imm;
                    break;
                case OPCODE_ORI:
                    EX_MEM.alu_result = ID_EX.reg_val_rs | E->imm;
                    break;
                case OPCODE_ANDI:
                    EX_MEM.alu_result = ID_EX.reg_val_rs & E->imm;
                    break;
                case OPCODE_XORI:
                    EX_MEM.alu_result = ID_EX.reg_val_rs ^ E->imm;
                    break;

                // LDW, STW
                case OPCODE_LDW:
                case OPCODE_STW:
                    EX_MEM.alu_result = ID_EX.reg_val_rs + E->imm;
                    break;

                // BZ, BEQ, JR
                case OPCODE_BZ:
                    EX_MEM.alu_result = (ID_EX.reg_val_rs == 0) ? 1 : 0;
                    break;
                case OPCODE_BEQ:
                    EX_MEM.alu_result = (ID_EX.reg_val_rs == ID_EX.reg_val_rt) ? 1 : 0;
                    break;
                case OPCODE_JR:
                    EX_MEM.alu_result = ID_EX.reg_val_rs;
                    break;

                case OPCODE_HALT:
                    EX_MEM.alu_result = 0;
                    break;

                default:
                    EX_MEM.alu_result = 0;
                    break;
            }
        } else {
            EX_MEM.instr.valid = false;
        }

        // --------------------------
        // 4) ID stage & Hazard Detection (No Forwarding)
        bool do_stall = false;
        if (IF_ID.instr.valid) {
            Instr *D = &IF_ID.instr;
            // Check against EX_MEM stage
            if (EX_MEM.instr.valid) {
                Instr *E = &EX_MEM.instr;
                uint8_t dest = (E->opcode <= OPCODE_XOR) ? E->rd
                                 : ((E->opcode <= OPCODE_XORI) ? E->rt
                                    : (E->opcode == OPCODE_LDW ? E->rt : 255));
                if (dest < 32 && (dest == D->rs || dest == D->rt)) {
                    do_stall = true;
                }
            }
            // If not stalled yet, check against ID_EX stage
            if (!do_stall && ID_EX.instr.valid) {
                Instr *P = &ID_EX.instr;
                uint8_t dest = (P->opcode <= OPCODE_XOR) ? P->rd
                                 : ((P->opcode <= OPCODE_XORI) ? P->rt
                                    : (P->opcode == OPCODE_LDW ? P->rt : 255));
                if (dest < 32 && (dest == D->rs || dest == D->rt)) {
                    do_stall = true;
                }
            }
        }

        if (do_stall) {
            // Insert NOP into EX stage, freeze IF/ID
            // (MEM→WB and EX→MEM already computed above)
            MEM_WB = MEM_WB;
            EX_MEM = EX_MEM;
            ID_EX.instr.valid = false; // bubble
            // IF_ID stays the same
            // PC does not advance
            continue;
        }

        // --------------------------
        // 5) IF stage: fetch
        Instr fetched = { .valid = false };
        if (PC != 0xFFFFFFFF) {
            if ((PC % 4) || (PC / 4 >= MEM_SIZE_WORDS)) {
                fprintf(stderr, "Fetch PC OOB in no-forward: 0x%08X\n", PC);
                exit(1);
            }
            fetched = decode_instr(Mem[PC / 4]);
        }
        uint32_t next_PC = (PC == 0xFFFFFFFF ? 0xFFFFFFFF : PC + 4);

        // Check if EX stage had a taken branch/jump
        if (EX_MEM.instr.valid) {
            Instr *E = &EX_MEM.instr;
            bool taken = false;
            uint32_t target = next_PC;
            switch (E->opcode) {
                case OPCODE_BZ:
                    if (EX_MEM.alu_result == 1) {
                        taken = true;
                        target = PC + (E->imm * 4);
                    }
                    break;
                case OPCODE_BEQ:
                    if (EX_MEM.alu_result == 1) {
                        taken = true;
                        target = PC + (E->imm * 4);
                    }
                    break;
                case OPCODE_JR:
                    taken = true;
                    target = (uint32_t)EX_MEM.alu_result;
                    break;
                case OPCODE_HALT:
                    taken = true;
                    break;
                default:
                    break;
            }
            if (taken) {
                IF_ID.instr.valid = false;
                ID_EX.instr.valid = false;
                next_PC = target;
            }
        }

        // If HALT reached WB, end simulation
        if (MEM_WB.instr.valid && MEM_WB.instr.opcode == OPCODE_HALT) {
            done = true;
            break;
        }

        // --------------------------
        // Advance pipeline latches
        MEM_WB = MEM_WB;
        EX_MEM = EX_MEM;

        // ID → EX
        ID_EX.instr = IF_ID.instr;
        if (IF_ID.instr.valid) {
            ID_EX.reg_val_rs = RegFile[IF_ID.instr.rs];
            ID_EX.reg_val_rt = RegFile[IF_ID.instr.rt];
        }

        // IF → ID
        IF_ID.instr = fetched;
        PC = next_PC;
    }

    printf("No-Forwarding: total_cycles = %llu\n", cycles_no_forward);
}

// -----------------------------------------------------------------------------
// “With-forwarding” 5-stage pipeline simulation.
// Populates cycles_forward, hazards_not_forwarded, then prints them.
void simulate_with_forwarding(const char *mem_image_path) {
    // Reload memory image
    load_memory_image(mem_image_path);

    // Initialize registers
    for (int i = 0; i < 32; i++) {
        RegFile[i] = 0;
    }

    // Pipeline latch initialization (all NOP)
    IFID_Reg  IF_ID  = { .instr.valid = false };
    IDEX_Reg  ID_EX  = { .instr.valid = false };
    EXMEM_Reg EX_MEM = { .instr.valid = false };
    MEMWB_Reg MEM_WB = { .instr.valid = false };

    uint32_t PC    = 0;
    bool     done  = false;
    cycles_forward = 0;
    hazards_not_forwarded = 0;

    while (!done) {
        cycles_forward++;

        // --------------------------
        // 1) WB stage
        if (MEM_WB.instr.valid) {
            Instr *W = &MEM_WB.instr;
            switch (W->opcode) {
                case OPCODE_ADD:
                case OPCODE_SUB:
                case OPCODE_MUL:
                case OPCODE_OR:
                case OPCODE_AND:
                case OPCODE_XOR:
                    RegFile[W->rd] = MEM_WB.alu_result;
                    break;
                case OPCODE_ADDI:
                case OPCODE_SUBI:
                case OPCODE_MULI:
                case OPCODE_ORI:
                case OPCODE_ANDI:
                case OPCODE_XORI:
                case OPCODE_LDW:
                    RegFile[W->rt] = MEM_WB.mem_data;
                    break;
                default:
                    break;
            }
        }

        // --------------------------
        // 2) MEM stage
        if (EX_MEM.instr.valid) {
            Instr *M = &EX_MEM.instr;
            MEM_WB = (MEMWB_Reg){ .instr = *M };
            switch (M->opcode) {
                case OPCODE_LDW: {
                    int32_t addr = EX_MEM.alu_result;
                    if (addr < 0 || (addr % 4) || (addr / 4) >= MEM_SIZE_WORDS) {
                        fprintf(stderr, "LDW address OOB in forward at cycle %llu\n",
                                cycles_forward);
                        exit(1);
                    }
                    MEM_WB.mem_data = (int32_t)Mem[addr / 4];
                    break;
                }
                case OPCODE_STW: {
                    int32_t addr = EX_MEM.alu_result;
                    if (addr < 0 || (addr % 4) || (addr / 4) >= MEM_SIZE_WORDS) {
                        fprintf(stderr, "STW address OOB in forward at cycle %llu\n",
                                cycles_forward);
                        exit(1);
                    }
                    Mem[addr / 4] = (uint32_t)EX_MEM.reg_val_rt;
                    MEM_WB.mem_data = 0;
                    break;
                }
                default:
                    MEM_WB.mem_data   = EX_MEM.alu_result;
                    break;
            }
            MEM_WB.alu_result = EX_MEM.alu_result;
        } else {
            MEM_WB.instr.valid = false;
        }

        // --------------------------
        // 3) EX stage (with forwarding)
        if (ID_EX.instr.valid) {
            Instr *E = &ID_EX.instr;
            EX_MEM = (EXMEM_Reg){ .instr = *E };
            int32_t op1 = ID_EX.reg_val_rs;
            int32_t op2 = ID_EX.reg_val_rt;

            // 3a) Forward from EX/MEM if possible
            if (EX_MEM.instr.valid) {
                Instr *F = &EX_MEM.instr;
                uint8_t fDest = (F->opcode <= OPCODE_XOR) ? F->rd
                                 : ((F->opcode <= OPCODE_XORI) ? F->rt
                                    : (F->opcode == OPCODE_LDW ? F->rt : 255));
                if (fDest < 32 && F->opcode <= OPCODE_XORI) {
                    if (fDest == E->rs) {
                        op1 = EX_MEM.alu_result;
                    }
                    if (fDest == E->rt) {
                        op2 = EX_MEM.alu_result;
                    }
                }
            }

            // 3b) Forward from MEM/WB if possible
            if (MEM_WB.instr.valid) {
                Instr *G = &MEM_WB.instr;
                uint8_t gDest = (G->opcode <= OPCODE_XOR) ? G->rd
                                 : ((G->opcode <= OPCODE_XORI) ? G->rt
                                    : (G->opcode == OPCODE_LDW ? G->rt : 255));
                if (gDest < 32) {
                    if (gDest == E->rs) {
                        op1 = (G->opcode == OPCODE_LDW) ? MEM_WB.mem_data : MEM_WB.alu_result;
                    }
                    if (gDest == E->rt) {
                        op2 = (G->opcode == OPCODE_LDW) ? MEM_WB.mem_data : MEM_WB.alu_result;
                    }
                }
            }

            // 3c) Perform ALU or branch computation
            switch (E->opcode) {
                case OPCODE_ADD:
                    EX_MEM.alu_result = op1 + op2;
                    break;
                case OPCODE_SUB:
                    EX_MEM.alu_result = op1 - op2;
                    break;
                case OPCODE_MUL:
                    EX_MEM.alu_result = op1 * op2;
                    break;
                case OPCODE_OR:
                    EX_MEM.alu_result = op1 | op2;
                    break;
                case OPCODE_AND:
                    EX_MEM.alu_result = op1 & op2;
                    break;
                case OPCODE_XOR:
                    EX_MEM.alu_result = op1 ^ op2;
                    break;
                case OPCODE_ADDI:
                    EX_MEM.alu_result = op1 + E->imm;
                    break;
                case OPCODE_SUBI:
                    EX_MEM.alu_result = op1 - E->imm;
                    break;
                case OPCODE_MULI:
                    EX_MEM.alu_result = op1 * E->imm;
                    break;
                case OPCODE_ORI:
                    EX_MEM.alu_result = op1 | E->imm;
                    break;
                case OPCODE_ANDI:
                    EX_MEM.alu_result = op1 & E->imm;
                    break;
                case OPCODE_XORI:
                    EX_MEM.alu_result = op1 ^ E->imm;
                    break;
                case OPCODE_LDW:
                case OPCODE_STW:
                    EX_MEM.alu_result = op1 + E->imm;
                    EX_MEM.reg_val_rt  = op2;  // STW needs this
                    break;
                case OPCODE_BZ:
                    EX_MEM.alu_result = (op1 == 0) ? 1 : 0;
                    break;
                case OPCODE_BEQ:
                    EX_MEM.alu_result = (op1 == op2) ? 1 : 0;
                    break;
                case OPCODE_JR:
                    EX_MEM.alu_result = op1;
                    break;
                case OPCODE_HALT:
                    EX_MEM.alu_result = 0;
                    break;
                default:
                    EX_MEM.alu_result = 0;
                    break;
            }
        } else {
            EX_MEM.instr.valid = false;
        }

        // --------------------------
        // 4) ID stage & Load-Use Hazard Detection (Forwarding)
        bool need_stall = false;
        if (IF_ID.instr.valid && EX_MEM.instr.valid) {
            Instr *D = &IF_ID.instr;
            Instr *P = &EX_MEM.instr;
            if (P->opcode == OPCODE_LDW) {
                uint8_t dest = P->rt;
                if (dest < 32 && (dest == D->rs || dest == D->rt)) {
                    need_stall = true;
                    hazards_not_forwarded++;
                }
            }
        }

        if (need_stall) {
            // Insert one bubble into EX stage, freeze IF/ID
            MEM_WB  = MEM_WB;
            EX_MEM  = EX_MEM;
            ID_EX.instr.valid = false;  // bubble
            // IF_ID stays same
            // PC does not advance
            continue;
        }

        // --------------------------
        // 5) IF stage: fetch next instruction
        Instr fetched = { .valid = false };
        if (PC != 0xFFFFFFFF) {
            if ((PC % 4) || (PC / 4 >= MEM_SIZE_WORDS)) {
                fprintf(stderr, "Fetch PC OOB in forward: 0x%08X\n", PC);
                exit(1);
            }
            fetched = decode_instr(Mem[PC / 4]);
        }
        uint32_t next_PC = (PC == 0xFFFFFFFF ? 0xFFFFFFFF : PC + 4);

        // Check for branch/jump in EX stage
        if (EX_MEM.instr.valid) {
            Instr *E = &EX_MEM.instr;
            bool taken = false;
            uint32_t target = next_PC;
            switch (E->opcode) {
                case OPCODE_BZ:
                    if (EX_MEM.alu_result == 1) {
                        taken = true;
                        target = PC + (E->imm * 4);
                    }
                    break;
                case OPCODE_BEQ:
                    if (EX_MEM.alu_result == 1) {
                        taken = true;
                        target = PC + (E->imm * 4);
                    }
                    break;
                case OPCODE_JR:
                    taken = true;
                    target = (uint32_t)EX_MEM.alu_result;
                    break;
                case OPCODE_HALT:
                    taken = true;
                    break;
                default:
                    break;
            }
            if (taken) {
                IF_ID.instr.valid = false;
                ID_EX.instr.valid = false;
                next_PC = target;
            }
        }

        // If HALT reached WB, end simulation
        if (MEM_WB.instr.valid && MEM_WB.instr.opcode == OPCODE_HALT) {
            done = true;
            break;
        }

        // --------------------------
        // Advance pipeline latches
        MEM_WB = MEM_WB;
        EX_MEM = EX_MEM;

        // ID → EX
        ID_EX.instr = IF_ID.instr;
        if (IF_ID.instr.valid) {
            ID_EX.reg_val_rs = RegFile[IF_ID.instr.rs];
            ID_EX.reg_val_rt = RegFile[IF_ID.instr.rt];
        }

        // IF → ID
        IF_ID.instr = fetched;
        PC = next_PC;
    }

    printf("Forwarding: total_cycles = %llu\n", cycles_forward);
    printf("Forwarding: hazards_not_eliminated = %llu\n", hazards_not_forwarded);
    if (cycles_forward > 0) {
        double speedup = ((double)cycles_no_forward) / ((double)cycles_forward);
        printf("Speedup = %.4f\n", speedup);
    }
}

// -----------------------------------------------------------------------------
// main(): call both simulators on the same memory image
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory_image.txt>\n", argv[0]);
        return 1;
    }

    simulate_no_forwarding(argv[1]);
    simulate_with_forwarding(argv[1]);
    return 0;
}
