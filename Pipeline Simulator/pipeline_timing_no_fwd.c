/*
 * pipeline_timing_no_forwarding.c
 *
 * Implements a 5-stage MIPS-lite pipeline timing simulator
 * assuming no forwarding (hazards incur stall cycles).
 * Leverages existing InstructionDecoder0.1 for instruction decoding.
 *
 * Covers Tasks 6 & 7 from the suggested breakdown
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//check this
#include "InstructionDecoder0.1.h"  // existing decoder definitions

#define MAX_INSTR 1024
#define NO_REG -1

// Simplified instruction struct for timing model
typedef struct {
    int index;       // instruction index (0-based)
    uint8_t opcode;  // 6-bit opcode
    int dest;        // destination register (NO_REG if none)
    int src1;        // source register 1 (NO_REG if none)
    int src2;        // source register 2 (NO_REG if none)
} Instr;

/**
 * simulate_no_forwarding:
 *   Given an array of Instr, computes total cycles, number of RAW hazards,
 *   and total stall cycles assuming no forwarding.
 */
void simulate_no_forwarding(Instr *instrs, int n,
                            int *total_cycles,
                            int *hazard_count,
                            int *total_stalls) {
    int *schedule = calloc(n, sizeof(int));
    if (!schedule) exit(1);

    *hazard_count = 0;
    *total_stalls = 0;

    // first instruction starts at cycle 1 (IF stage)
    schedule[0] = 1;
    for (int i = 1; i < n; i++) {
        int init = schedule[i - 1] + 1;
        int stalls = 0;
        // check RAW hazards vs all prior instructions
        for (int j = 0; j < i; j++) {
            int prod = instrs[j].dest;
            if (prod == NO_REG) continue;
            if (instrs[i].src1 == prod || instrs[i].src2 == prod) {
                int needed = schedule[j] + 3;
                if (init < needed) {
                    int s = needed - init;
                    if (s > stalls) stalls = s;
                }
            }
        }
        if (stalls > 0) {
            (*hazard_count)++;
            *total_stalls += stalls;
        }
        schedule[i] = init + stalls;
    }
    // WB of last instr at schedule[n-1] + 4
    *total_cycles = schedule[n - 1] + 4;
    free(schedule);
}

/**
 * load_trace: loads up to MAX_INSTR hex words (one per line) into buffer
 * Returns number of words read or -1 on error.
 */
int load_trace(const char *path, uint32_t *buffer) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int count = 0;
    while (count < MAX_INSTR && fscanf(f, "%8" SCNx32, &buffer[count]) == 1) {
        count++;
    }
    fclose(f);
    return count;
}

/**
 * run_tests: simple sanity checks for no-forwarding simulator
 */
void run_tests() {
    printf("Running timing simulator tests...\n");
    {
        Instr a[] = {{0, 0, 1, 2, 3}, {1, 0, 4, 5, 6}, {2, 0, 7, 8, 9}};
        int cyc, hz, st;
        simulate_no_forwarding(a, 3, &cyc, &hz, &st);
        printf("Test1: cycles=%d, hazards=%d, stalls=%d (expect 7,0,0)\n", cyc, hz, st);
    }
    {
        Instr a[] = {{0, 0, 1, 2, 3}, {1, 0, 4, 1, 6}};
        int cyc, hz, st;
        simulate_no_forwarding(a, 2, &cyc, &hz, &st);
        printf("Test2: cycles=%d, hazards=%d, stalls=%d (expect haz=1,st=2)\n", cyc, hz, st);
    }
    {
        Instr a[] = {{0, 0, 1, 2, 3}, {1, 0, 5, 6, 7}, {2, 0, 8, 1, 9}};
        int cyc, hz, st;
        simulate_no_forwarding(a, 3, &cyc, &hz, &st);
        printf("Test3: hazards=%d, stalls=%d (expect haz=1,st=1)\n", hz, st);
    }
    printf("Tests complete.\n");
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        run_tests();
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <trace_file> [--test]\n", argv[0]);
        return 1;
    }

    uint32_t trace[MAX_INSTR];
    int n = load_trace(argv[1], trace);
    if (n < 0) {
        perror("Failed to open trace");
        return 1;
    }

    Instr *instrs = calloc(n, sizeof(Instr));
    if (!instrs) exit(1);

    // Decode each raw word using existing decoder
    for (int i = 0; i < n; i++) {
        DecodedInstruction di = decode_instruction(trace[i]);
        Instr ins = {i, (uint8_t)di.opcode, NO_REG, NO_REG, NO_REG};

        if (di.type == R_TYPE) {
            ins.src1 = di.rs;
            ins.src2 = di.rt;
            ins.dest = di.rd;
        } else if (di.type == I_TYPE) {
            ins.src1 = di.rs;
            switch ((Opcode)di.opcode) {
                case ADDI:
                case SUBI:
                case MULI:
                case ORI:
                case ANDI:
                case XORI:
                case LDW:
                    ins.dest = di.rt;
                    break;
                case STW:
                    ins.src2 = di.rt;
                    break;
                case BEQ:
                    ins.src2 = di.rt;
                    break;
                case BZ:
                case JR:
                case HALT:
                default:
                    // no additional regs
                    break;
            }
        }
        instrs[i] = ins;
    }

    int cycles, hazards, stalls;
    simulate_no_forwarding(instrs, n, &cycles, &hazards, &stalls);

    printf("No-forwarding timing results:\n");
    printf("  Instructions    : %d\n", n);
    printf("  Total cycles    : %d\n", cycles);
    printf("  Data hazards    : %d\n", hazards);
    printf("  Stall cycles    : %d\n", stalls);
    printf("  Avg stall/hazard: %.2f\n", hazards ? (double)stalls / hazards : 0.0);

    free(instrs);
    return 0;
}
