#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instruction_decoder.h"
#include "functional_sim.h"
#include "no_fwd.h"

#define PIPELINE_DEPTH 5

// Global clock cycle count
int clock_cycles = 0;
int total_stalls = 0;
int total_flushes = 0;

// Struct to hold pipeline state
typedef struct {
    DecodedInstruction instr;
    int valid; // 1 if real instruction, 0 if NOP or flushed
    uint32_t pc; // Program counter of this instruction
} PipelineRegister;

// Circular buffer for pipeline
PipelineRegister pipeline[PIPELINE_DEPTH];

// Internal PC for pipeline
uint32_t pipeline_pc = 0;
int pipeline_halt_seen = 0;

// Check if instruction is NOP
int is_nop(DecodedInstruction instr) {
    return instr.opcode == NOP;
}

// Insert a NOP into a pipeline stage
void insert_nop(int stage) {
    pipeline[stage].instr.opcode = NOP;
    pipeline[stage].instr.rs = pipeline[stage].instr.rt = pipeline[stage].instr.rd = 0;
    pipeline[stage].instr.immediate = 0;
    pipeline[stage].valid = 0;
}

// Initialize pipeline to NOPs
void initialize_pipeline() {
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        insert_nop(i);
    }
    pipeline_pc = 0;
    pipeline_halt_seen = 0;
}

// Advance pipeline (shift right)
void advance_pipeline() {
    for (int i = WB; i > IF; i--) {
        memcpy(&pipeline[i], &pipeline[i - 1], sizeof(PipelineRegister));
    }
    insert_nop(IF); // Clear the IF stage after advancing
}

// Detect RAW hazard between ID and EX/MEM/WB
int detect_raw_hazard(DecodedInstruction curr, DecodedInstruction ex, DecodedInstruction mem, DecodedInstruction wb) {
    if (is_nop(curr)) return 0;

    int hazard = 0;
    int src1 = curr.rs;
    int src2 = (curr.type == R_TYPE || curr.opcode == BEQ) ? curr.rt : -1;

    int ex_dest = (ex.type == R_TYPE) ? ex.rd : ex.rt;
    int mem_dest = (mem.type == R_TYPE) ? mem.rd : mem.rt;
    int wb_dest = (wb.type == R_TYPE) ? wb.rd : wb.rt;

    if (pipeline[EX].valid && (ex_dest == src1 || ex_dest == src2)) hazard = 1;
    if (pipeline[MEM].valid && (mem_dest == src1 || mem_dest == src2)) hazard = 1;
    if (pipeline[WB].valid && (wb_dest == src1 || wb_dest == src2)) hazard = 1;

    return hazard;
}

// Simulate pipeline with no forwarding
void simulate_pipeline_no_forwarding() {
    initialize_pipeline();

    while (!pipeline_halt_seen || pipeline[IF].valid || pipeline[ID].valid ||
           pipeline[EX].valid || pipeline[MEM].valid || pipeline[WB].valid) {

        clock_cycles++;

        // Debugging output
        printf("Clock cycle: %d\n", clock_cycles);
        printf("Pipeline state: IF=%d, ID=%d, EX=%d, MEM=%d, WB=%d\n",
               pipeline[IF].instr.opcode, pipeline[ID].instr.opcode,
               pipeline[EX].instr.opcode, pipeline[MEM].instr.opcode,
               pipeline[WB].instr.opcode);
        printf("Pipeline PC: %u\n", pipeline_pc);

        // WB stage
        if (pipeline[WB].valid && !is_nop(pipeline[WB].instr)) {
            simulate_instruction(pipeline[WB].instr);
            if (pipeline[WB].instr.opcode == HALT) {
                pipeline_halt_seen = 1;
            }
        }

        // Handle taken branches in EX stage 
        if (pipeline[EX].valid &&
            (pipeline[EX].instr.opcode == BEQ || pipeline[EX].instr.opcode == BZ || pipeline[EX].instr.opcode == JR)) {

            if (pipeline[EX].instr.branch_taken) {
                pipeline_pc = pipeline[EX].instr.branch_target;
                insert_nop(IF);
                insert_nop(ID);
                total_flushes += 2;

                // Debugging output for branch handling
                printf("Branch taken in EX stage. Flushing IF and ID. New PC: %u\n", pipeline_pc);
            }
        }

        // Detect RAW hazard
        int stall = 0;
        if (pipeline[ID].valid &&
            pipeline[ID].instr.opcode != HALT &&
            !is_nop(pipeline[ID].instr)) {

            stall = detect_raw_hazard(
                pipeline[ID].instr,
                pipeline[EX].instr,
                pipeline[MEM].instr,
                pipeline[WB].instr
            );

            // Debugging output for RAW hazard detection
            if (stall) {
                printf("RAW hazard detected. Stalling pipeline.\n");
            }
        }

        if (stall) {
            // Stall pipeline: only move MEM/WB, insert NOP into EX
            pipeline[WB] = pipeline[MEM];
            pipeline[MEM] = pipeline[EX];
            insert_nop(EX);  // Freeze ID/IF
            total_stalls++;

            // Debugging output for stalling
            printf("Pipeline stalled. Inserting NOP into EX stage.\n");
        } else {
            // Advance pipeline
            advance_pipeline();

            // Fetch stage
            if (!pipeline_halt_seen && pipeline_pc < 4096) {
                uint32_t instr_word = state.memory[pipeline_pc / 4];
                DecodedInstruction fetched = decode_instruction(instr_word);

                pipeline[IF].instr = fetched;
                pipeline[IF].valid = 1;
                pipeline[IF].pc = pipeline_pc;
                pipeline_pc += 4;

                // Debugging output for instruction fetch
                printf("Fetched instruction at PC: %u. Opcode: %d\n", pipeline_pc, fetched.opcode);
            } else {
                insert_nop(IF);

                // Debugging output for inserting NOP in IF stage
                printf("Inserting NOP into IF stage. PC: %u\n", pipeline_pc);
            }
        }
    }

    // Final statistics
    printf("Total stalls: %d\n", total_stalls);
    printf("Total flushes: %d\n", total_flushes);
    printf("\nTiming Simulator:\n");
    printf("Total number of clock cycles: %d\n", clock_cycles);
}