#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instruction_decoder.h"
#include "functional_sim.h" // Assuming this has MachineState state; and register_written
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
    pipeline[stage].instr.type = R_TYPE; // NOPs can be R_TYPE, won't affect anything
    pipeline[stage].instr.rs = pipeline[stage].instr.rt = pipeline[stage].instr.rd = 0;
    pipeline[stage].instr.immediate = 0;
    pipeline[stage].valid = 0;
    pipeline[stage].pc = 0; // Clear PC for NOPs
}

// Initialize pipeline to NOPs
void initialize_pipeline() {
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        insert_nop(i);
    }
    pipeline_pc = 0;
    pipeline_halt_seen = 0;
    clock_cycles = 0; // Reset clock cycles
    total_stalls = 0; // Reset stalls
    total_flushes = 0; // Reset flushes
}

// Advance pipeline (shift right) - This function will be modified or its use cases refined
// as the main loop will handle conditional advancements.
// For now, it will be kept for when no stall/flush occurs.
void advance_pipeline() {
    for (int i = WB; i > IF; i--) {
        memcpy(&pipeline[i], &pipeline[i - 1], sizeof(PipelineRegister));
    }
    insert_nop(IF); // Clear the IF stage after advancing
}

// Detect RAW hazard between ID and EX/MEM
int detect_raw_hazard(DecodedInstruction curr_id, DecodedInstruction ex, DecodedInstruction mem) {
    if (is_nop(curr_id) || curr_id.opcode == HALT) return 0; // NOPs and HALT don't cause hazards

    int hazard = 0;
    int src1 = curr_id.rs;
    int src2 = -1; // Default for non-R-type or non-BEQ

    // Determine source registers for current instruction in ID stage
    if (curr_id.type == R_TYPE || curr_id.opcode == BEQ) {
        src2 = curr_id.rt;
    }
    // Note: R0 (register 0) is never written to and reads as 0, so it can't cause a RAW hazard as a source.
    if (src1 == 0) src1 = -1; // Treat R0 as not a source for hazard detection
    if (src2 == 0) src2 = -1; // Treat R0 as not a source for hazard detection

    // Check EX stage producer
    int ex_dest = -1;
    if (pipeline[EX].valid && !is_nop(ex)) {
        if (ex.type == R_TYPE) {
            ex_dest = ex.rd;
        } else if (ex.opcode == ADDI || ex.opcode == SUBI || ex.opcode == MULI ||
                   ex.opcode == ORI || ex.opcode == ANDI || ex.opcode == XORI || ex.opcode == LDW) {
            ex_dest = ex.rt;
        }
        if (ex_dest != -1 && ex_dest != 0) { // Don't consider R0 as a destination for hazards
            if (ex_dest == src1 || ex_dest == src2) {
                hazard = 1; // Hazard: ID needs data from EX (needs 2 stalls)
            }
        }
    }

    // Check MEM stage producer
    int mem_dest = -1;
    if (pipeline[MEM].valid && !is_nop(mem)) {
        if (mem.type == R_TYPE) {
            mem_dest = mem.rd;
        } else if (mem.opcode == ADDI || mem.opcode == SUBI || mem.opcode == MULI ||
                   mem.opcode == ORI || mem.opcode == ANDI || mem.opcode == XORI || mem.opcode == LDW) {
            mem_dest = mem.rt;
        }
        if (mem_dest != -1 && mem_dest != 0) { // Don't consider R0 as a destination for hazards
            if (mem_dest == src1 || mem_dest == src2) {
                hazard = 1; // Hazard: ID needs data from MEM (needs 1 stall)
            }
        }
    }

    // Per project description, data written in WB is available in the same cycle for ID reads.
    // So, no RAW hazard for WB stage producers.

    return hazard;
}

// Simulate pipeline with no forwarding
void simulate_pipeline_no_forwarding() {
    initialize_pipeline(); //

    while (!pipeline_halt_seen || pipeline[IF].valid || pipeline[ID].valid ||
           pipeline[EX].valid || pipeline[MEM].valid || pipeline[WB].valid) {

        clock_cycles++; // Increment clock cycle at the start of each cycle

        // Debugging output for current cycle
        printf("Clock cycle: %d\n", clock_cycles);
        printf("Pipeline state: IF=%d, ID=%d, EX=%d, MEM=%d, WB=%d\n",
               pipeline[IF].instr.opcode, pipeline[ID].instr.opcode,
               pipeline[EX].instr.opcode, pipeline[MEM].instr.opcode,
               pipeline[WB].instr.opcode);
        printf("Pipeline PC: %u\n", pipeline_pc);

        // 1. WB stage execution: Instructions commit and update architectural state
        if (pipeline[WB].valid && !is_nop(pipeline[WB].instr)) { //
            simulate_instruction(pipeline[WB].instr); // This updates state.registers and state.memory
            if (pipeline[WB].instr.opcode == HALT) { //
                pipeline_halt_seen = 1; // Mark that HALT has reached WB
            }
        }

        // Flags to control pipeline movement this cycle
        int branch_flush_this_cycle = 0; //
        int raw_hazard_stall_this_cycle = 0; //

        // 2. Branch Handling in EX stage: Check for taken branches/jumps and flush
        if (pipeline[EX].valid && !is_nop(pipeline[EX].instr) && //
            (pipeline[EX].instr.opcode == BEQ || pipeline[EX].instr.opcode == BZ || pipeline[EX].instr.opcode == JR)) { //

            int is_branch_taken = 0; //
            uint32_t target_pc = 0; //

            // Evaluate branch condition using current architectural state (state.registers)
            if (pipeline[EX].instr.opcode == BZ) { //
                if (state.registers[pipeline[EX].instr.rs] == 0) { //
                    is_branch_taken = 1; //
                    // Branch target: PC of current instruction + 4 (next sequential) + immediate * 4 (offset)
                    target_pc = pipeline[EX].pc + 4 + (pipeline[EX].instr.immediate * 4); //
                }
            } else if (pipeline[EX].instr.opcode == BEQ) { //
                if (state.registers[pipeline[EX].instr.rs] == state.registers[pipeline[EX].instr.rt]) { //
                    is_branch_taken = 1; //
                    target_pc = pipeline[EX].pc + 4 + (pipeline[EX].instr.immediate * 4); //
                }
            } else if (pipeline[EX].instr.opcode == JR) { //
                is_branch_taken = 1; // JR is always taken
                target_pc = state.registers[pipeline[EX].instr.rs]; // Target is value in Rs
            }

            if (is_branch_taken) { //
                pipeline_pc = target_pc; // Update the pipeline's PC for next fetch
                insert_nop(IF); // Flush IF stage
                insert_nop(ID); // Flush ID stage
                total_flushes += 2; // Account for two flushed instructions
                branch_flush_this_cycle = 1; // Indicate a flush occurred
                printf("Branch taken in EX stage. Flushing IF and ID. New PC: %u\n", pipeline_pc); //
            }
        }

        // 3. Detect RAW hazard in ID stage
        if (pipeline[ID].valid && !is_nop(pipeline[ID].instr) && pipeline[ID].instr.opcode != HALT) { //
            raw_hazard_stall_this_cycle = detect_raw_hazard( //
                pipeline[ID].instr,
                pipeline[EX].instr,
                pipeline[MEM].instr
                // WB stage is not checked for RAW hazard in no-forwarding, as per problem spec
            );

            if (raw_hazard_stall_this_cycle) { //
                printf("RAW hazard detected. Stalling pipeline.\n"); //
                total_stalls++; // Increment stall counter
            }
        }

        // 4. Advance/Stall pipeline stages based on flags
        if (raw_hazard_stall_this_cycle) { //
            // When stalling due to RAW, IF and ID remain in place.
            // MEM and WB advance normally. EX receives a NOP.
            pipeline[WB] = pipeline[MEM]; //
            pipeline[MEM] = pipeline[EX]; //
            insert_nop(EX);  // EX becomes NOP
            // IF and ID are implicitly held as they are not updated from their left
            printf("Pipeline stalled. Inserting NOP into EX stage.\n"); //
        } else {
            // Normal advancement: All stages shift
            pipeline[WB] = pipeline[MEM]; //
            pipeline[MEM] = pipeline[EX]; //
            pipeline[EX] = pipeline[ID]; //
            pipeline[ID] = pipeline[IF]; //
            insert_nop(IF); // IF is cleared for new fetch
        }

        // 5. Fetch new instruction into IF stage
        // Fetch only if not stalled, and program has not halted, and PC is within bounds
        // If a branch was taken and flushed, pipeline_pc is already updated to the correct target.
        if (!raw_hazard_stall_this_cycle && !pipeline_halt_seen && pipeline_pc < 4096) { //
            uint32_t instr_word = state.memory[pipeline_pc / 4]; //
            DecodedInstruction fetched = decode_instruction(instr_word); //

            pipeline[IF].instr = fetched; //
            pipeline[IF].valid = 1; //
            pipeline[IF].pc = pipeline_pc; // Store PC of this instruction
            pipeline_pc += 4; // Increment PC for the *next* fetch

            printf("Fetched instruction at PC: %u. Opcode: %d\n", pipeline[IF].pc, fetched.opcode); // Use pipeline[IF].pc for current instruction's PC
        } else if (!raw_hazard_stall_this_cycle) { // If not stalled, but cannot fetch (e.g., pipeline_pc >= 4096 or halt seen)
            insert_nop(IF); // Insert NOP to clear IF stage
            printf("Inserting NOP into IF stage because no fetch occurred. PC: %u\n", pipeline_pc); //
        }
    }

    // Final statistics
    printf("Total stalls: %d\n", total_stalls); //
    printf("Total flushes: %d\n", total_flushes); //
    printf("\nTiming Simulator:\n"); //
    printf("Total number of clock cycles: %d\n", clock_cycles); //
}
