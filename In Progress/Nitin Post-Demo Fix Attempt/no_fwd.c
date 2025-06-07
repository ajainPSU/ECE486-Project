#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instruction_decoder.h"
#include "functional_sim.h"
#include "no_fwd.h"
#include "trace_reader.h" // Needed for MAX_MEMORY_LINES and WORD_SIZE

#define PIPELINE_DEPTH 5

// Global clock cycle count (declared extern in functional_sim.h, defined in global_counters.c)
extern int clock_cycles;
extern int total_stalls;
extern int total_flushes;

// Global NOP_INSTRUCTION instance (declared extern in no_fwd.h, defined in global_counters.c)
extern DecodedInstruction NOP_INSTRUCTION;

// Circular buffer for pipeline - MAKE STATIC
static PipelineRegister pipeline[PIPELINE_DEPTH];

// Internal PC for pipeline - MAKE STATIC
static uint32_t pipeline_pc = 0; // This tracks the PC of the instruction to be fetched NEXT
static int pipeline_halt_seen = 0; // Flag to indicate if HALT instruction has been fetched


// Check if instruction is NOP
int is_nop(DecodedInstruction instr) {
    return instr.opcode == NOP;
}

// Insert a NOP into a pipeline stage
void insert_nop(int stage, PipelineRegister pipeline_arr[]) {
    pipeline_arr[stage].instr = NOP_INSTRUCTION;
    pipeline_arr[stage].valid = 0; // Mark as invalid (NOP)
    pipeline_arr[stage].pc = 0; // Clear PC for NOPs
    pipeline_arr[stage].branch_taken = 0; // Clear branch flags
    pipeline_arr[stage].branch_target = 0;
    pipeline_arr[stage].result_val = 0; // Clear result
}

// Initialize pipeline to NOPs
void initialize_pipeline(PipelineRegister pipeline_arr[]) {
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        insert_nop(i, pipeline_arr);
    }
    pipeline_pc = 0; // Start fetching from address 0
    pipeline_halt_seen = 0;
    // NOTE: These counters are global and reset by global_counters.c if that's where they are defined,
    // or by functional_sim's initialize_machine_state().
    // If you want them specifically reset for EACH pipeline run (NF or WF),
    // then they would need to be local to this file or passed around.
    // For now, assuming they are truly global and persist between mode runs unless `initialize_machine_state` resets them.
    // However, for the output requirement of "Total stalls" and "Total number of clock cycles" specific to the *current* run,
    // it's better to reset them explicitly here at the start of each simulation.
    // I'll keep the explicit resets here.
    clock_cycles = 0;
    total_stalls = 0;
    total_flushes = 0;
    total_instructions = 0; // Reset functional sim's instruction counters
    arithmetic_instructions = 0;
    logical_instructions = 0;
    memory_access_instructions = 0;
    control_transfer_instructions = 0;
}

// Function to check if a register is a destination register for an instruction
int get_dest_reg(DecodedInstruction instr) {
    if (instr.type == R_TYPE) {
        return instr.rd;
    } else if (instr.opcode == ADDI || instr.opcode == SUBI || instr.opcode == MULI ||
               instr.opcode == ORI || instr.opcode == ANDI || instr.opcode == XORI ||
               instr.opcode == LDW) {
        return instr.rt;
    }
    return -1; // No destination register
}

// Function to check if a register is a source register for an instruction
int is_source_reg(DecodedInstruction instr, int reg_num) {
    if (reg_num == 0) return 0;

    if (instr.type == R_TYPE) {
        return (reg_num == instr.rs || reg_num == instr.rt);
    } else if (instr.type == I_TYPE) {
        if (instr.opcode == BZ) {
            return (reg_num == instr.rs);
        } else if (instr.opcode == BEQ) {
            return (reg_num == instr.rs || reg_num == instr.rt);
        } else if (instr.opcode == JR) {
            return (reg_num == instr.rs);
        } else {
            return (reg_num == instr.rs || (instr.opcode == STW && reg_num == instr.rt));
        }
    }
    return 0;
}

// Detect RAW hazard (now expects PipelineRegister as first arg)
int detect_raw_hazard(PipelineRegister curr_id_reg, PipelineRegister ex_reg, PipelineRegister mem_reg) {
    if (!curr_id_reg.valid || is_nop(curr_id_reg.instr) || curr_id_reg.instr.opcode == HALT) return 0;

    DecodedInstruction curr_id_instr = curr_id_reg.instr;
    int src1 = curr_id_instr.rs;
    int src2 = -1;

    if (curr_id_instr.type == R_TYPE || curr_id_instr.opcode == BEQ) {
        src2 = curr_id_instr.rt;
    } else if (curr_id_instr.opcode == STW) { // <<< ADD THIS ELSE IF BLOCK
        src2 = curr_id_instr.rt; // STW also uses rt as a source register (value to store)
    }

    if (src1 == 0) src1 = -1; // R0 is not a hazard source/dest
    if (src2 == 0) src2 = -1;

    int ex_dest = -1;
    // The filter on producer instructions (ex_reg.instr.opcode != BZ etc.) might still be something to review later
    // but let's ensure sources are correct first.
    if (ex_reg.valid && !is_nop(ex_reg.instr) /* && ex_reg.instr.opcode != HALT ... (existing filters) */ ) {
        ex_dest = get_dest_reg(ex_reg.instr);
        if (ex_dest == 0) ex_dest = -1; // Do not consider R0 as a written destination
        if (ex_dest != -1 && (ex_dest == src1 || (src2 != -1 && ex_dest == src2))) { // Check src2 only if valid
            return 1;
        }
    }

    int mem_dest = -1;
    if (mem_reg.valid && !is_nop(mem_reg.instr) /* && mem_reg.instr.opcode != HALT ... (existing filters) */ ) {
        mem_dest = get_dest_reg(mem_reg.instr);
        if (mem_dest == 0) mem_dest = -1; // Do not consider R0 as a written destination
        if (mem_dest != -1 && (mem_dest == src1 || (src2 != -1 && mem_dest == src2))) { // Check src2 only if valid
            return 1;
        }
    }
    return 0;
}

// Simulate one clock cycle of the pipeline (No Forwarding)
void simulate_one_cycle_no_forwarding_internal() {
    clock_cycles++;

    // Debugging Statements
    // --- Optional: Print header for the current cycle ---
    printf("Clock cycle: %d\n", clock_cycles);
    printf("  Reg State: R1=%d, R2=%d, R3=%d, R4=%d, R5=%d, R6=%d, R7=%d, R8=%d, R9=%d, R10=%d, R11=%d, R12=%d, R13=%d, R14=%d, R15=%d\n",
           state.registers[1], state.registers[2], state.registers[3], state.registers[4], state.registers[5],
           state.registers[6], state.registers[7], state.registers[8], state.registers[9], state.registers[10],
           state.registers[11], state.registers[12], state.registers[13], state.registers[14], state.registers[15]);

    printf("Pipeline state: IF=%s, ID=%s, EX=%s, MEM=%s, WB=%s\n",
           opcode_to_string(pipeline[IF].instr.opcode), opcode_to_string(pipeline[ID].instr.opcode),
           opcode_to_string(pipeline[EX].instr.opcode), opcode_to_string(pipeline[MEM].instr.opcode),
           opcode_to_string(pipeline[WB].instr.opcode));
    printf("Pipeline PC: %u\n", pipeline_pc);

    // Debugging PC at 88
    if (pipeline[WB].valid && pipeline[WB].pc == 88) {
        printf("DEBUG WB (Cycle %d): PC=%u, Opcode in pipeline[WB].instr = 0x%X, Expected STW (0x0D)\n",
            clock_cycles, pipeline[WB].pc, pipeline[WB].instr.opcode);

    }

    // --- Stage Execution (in reverse order for pipeline integrity) ---
    // 1. WB stage execution: Instructions commit and update architectural state
    if (pipeline[WB].valid && !is_nop(pipeline[WB].instr)) {
        // --- ADD THIS DEBUG BLOCK ---
        if (pipeline[WB].instr.opcode == HALT) {
            printf("[PIPE_DEBUG] HALT in WB. pipeline[WB].pc = 0x%X. Current state.pc BEFORE assignment = 0x%X\n", 
                   pipeline[WB].pc, state.pc);
        }
        // --- END DEBUG BLOCK ---

        state.pc = pipeline[WB].pc;

        // --- ADD THIS DEBUG BLOCK ---
        if (pipeline[WB].instr.opcode == HALT) {
            printf("[PIPE_DEBUG] HALT in WB. state.pc AFTER assignment (entry to simulate_instruction) = 0x%X\n", 
                   state.pc);
        }
        // --- END DEBUG BLOCK ---

        simulate_instruction(pipeline[WB].instr);
    } 

    int raw_hazard_stall_this_cycle = 0;
    int branch_flush_this_cycle = 0;

    // 2. Branch Resolution in EX stage
    if (pipeline[EX].valid && !is_nop(pipeline[EX].instr) &&
        (pipeline[EX].instr.opcode == BEQ || pipeline[EX].instr.opcode == BZ || pipeline[EX].instr.opcode == JR)) {

        // DEBUG Statement
        printf("  Branch check in EX: PC=%u, Opcode=%d\n", pipeline[EX].pc, pipeline[EX].instr.opcode);

        int is_branch_taken = 0;
        uint32_t branch_resolved_target_pc = 0;

        if (pipeline[EX].instr.opcode == BZ) {
            if (state.registers[pipeline[EX].instr.rs] == 0) {
                is_branch_taken = 1;
                // Corrected target calculation: PC_of_branch + (immediate_word_offset * 4)
                branch_resolved_target_pc = pipeline[EX].pc + ((int32_t)pipeline[EX].instr.immediate * 4); 
            }
        } else if (pipeline[EX].instr.opcode == BEQ) {
            if (state.registers[pipeline[EX].instr.rs] == state.registers[pipeline[EX].instr.rt]) {
                is_branch_taken = 1;
                // Corrected target calculation: PC_of_branch + (immediate_word_offset * 4)
                branch_resolved_target_pc = pipeline[EX].pc + ((int32_t)pipeline[EX].instr.immediate * 4); 
            }
        } else if (pipeline[EX].instr.opcode == JR) { // Ensure JR is handled correctly too
            is_branch_taken = 1;
            branch_resolved_target_pc = (uint32_t)state.registers[pipeline[EX].instr.rs];
        }

        if (is_branch_taken) {
            pipeline_pc = branch_resolved_target_pc;
            branch_flush_this_cycle = 1;
            total_flushes += 2;
            // DEBUG Statement
            printf("Branch taken in EX stage. Flushing IF and ID. New PC: %u\n", pipeline_pc);
        }
    }

    // 3. Detect RAW hazard in ID stage
    if (pipeline[ID].valid && !is_nop(pipeline[ID].instr) && pipeline[ID].instr.opcode != HALT) {
        raw_hazard_stall_this_cycle = detect_raw_hazard(
            pipeline[ID], // Pass the full PipelineRegister struct
            pipeline[EX],
            pipeline[MEM]
        );

        if (raw_hazard_stall_this_cycle) {
            // DEBUG Statement
            printf("RAW hazard detected. Stalling pipeline.\n");
            total_stalls++;
        }
    }

    // 4. Advance/Stall pipeline stages
    pipeline[WB] = pipeline[MEM];

    if (raw_hazard_stall_this_cycle) {
        pipeline[MEM] = pipeline[EX];
        insert_nop(EX, pipeline);
        // DEBUG Statement
        printf("Pipeline stalled. Inserting NOP into EX stage.\n");
    } else if (branch_flush_this_cycle) {
        pipeline[MEM] = pipeline[EX];
        insert_nop(EX, pipeline);
        insert_nop(ID, pipeline);
        insert_nop(IF, pipeline);
    }
    else {
        pipeline[MEM] = pipeline[EX];
        pipeline[EX] = pipeline[ID];
        pipeline[ID] = pipeline[IF];
        insert_nop(IF, pipeline);
    }

    // 5. Fetch new instruction into IF stage
    if (!raw_hazard_stall_this_cycle && !pipeline_halt_seen && pipeline_pc < (MAX_MEMORY_LINES * WORD_SIZE)) {
        uint32_t instr_word = state.memory[pipeline_pc / WORD_SIZE];
        DecodedInstruction fetched = decode_instruction(instr_word);

        pipeline[IF].instr = fetched;
        pipeline[IF].valid = 1;
        pipeline[IF].pc = pipeline_pc;

        if (fetched.opcode == HALT) {
            // DEBUG Statement
            printf("HALT instruction fetched. Stopping further fetches.\n");
            pipeline_halt_seen = 1;
        }

        pipeline_pc += WORD_SIZE;

        // DEBUG Statement
        printf("Fetched instruction at PC: %u. Opcode: %d\n", pipeline[IF].pc, fetched.opcode);
    } else if (!raw_hazard_stall_this_cycle && pipeline_halt_seen) {
        insert_nop(IF, pipeline);
        // DEBUG Statement
        printf("Inserting NOP into IF stage because HALT was previously fetched and no stall/flush.\n");
    } else if (!raw_hazard_stall_this_cycle && !pipeline_halt_seen && pipeline_pc >= (MAX_MEMORY_LINES * WORD_SIZE)) {
        insert_nop(IF, pipeline);
        // DEBUG Statement
        printf("Inserting NOP into IF stage because PC (%u) is out of memory bounds, effectively halting.\n", pipeline_pc);
        pipeline_halt_seen = 1;
    }
}

// In no_fwd_v6.c
void simulate_pipeline_no_forwarding() {
    initialize_pipeline(pipeline);
    int final_halt_processed_in_wb = 0; // Flag

    while (1) {
        /*if (final_halt_processed_in_wb) { // If HALT was processed last cycle, ensure we break now
            break;
        } */
        simulate_one_cycle_no_forwarding_internal(); 

        if (pipeline[WB].valid && pipeline[WB].instr.opcode == HALT) {
            // We've just simulated HALT in WB. Its architectural effects are now in globals.
            // Set a flag to break in the *next* iteration, allowing pipeline to empty if needed for counts.
            // Or, if pipeline is truly empty now except for HALT in WB, we can break.
            final_halt_processed_in_wb = 1; // Signal that HALT has been architecturally processed
            state.pc += 4;
            total_instructions++;
            control_transfer_instructions++;
            break;
        }

        int active_instructions_remaining = 0;
        for (int i = 0; i < PIPELINE_DEPTH; i++) {
            // Count active if not HALT in WB, or if HALT is not the *only* thing
            if (pipeline[i].valid && pipeline[i].instr.opcode != NOP) {
                 if (!(i == WB && pipeline[i].instr.opcode == HALT && final_halt_processed_in_wb)) {
                    active_instructions_remaining = 1;
                    break;
                 }
            }
        }
        
        if (final_halt_processed_in_wb && !active_instructions_remaining) {
            break; // HALT has committed, pipeline is now empty past it.
        }
        
        if (!active_instructions_remaining && pipeline_halt_seen) { // General drain condition
             break;
        }

        if (clock_cycles > 200000) {
            fprintf(stderr, "Simulator possibly in infinite loop, breaking.\n");
            break;
        }
    }
    // This print_final_state() should now have the correct values
    // because simulate_instruction(HALT) updated them, and then the loop broke.
    // Cheap solution

    print_final_state(); 
    // Note: Not iterating PC by 4 again, or instruction counts by 1 again.
}

/*
// Moved to comment for debugging purposes
// Main function to run the simulator (no forwarding)
void simulate_pipeline_no_forwarding() {
    initialize_pipeline(pipeline);

    while (1) {
        simulate_one_cycle_no_forwarding_internal();

        int active_instructions_remaining = 0;
        for (int i = 0; i < PIPELINE_DEPTH; i++) {
            if (pipeline[i].valid && pipeline[i].instr.opcode != NOP) {
                active_instructions_remaining = 1;
                break;
            }
        }

        if (pipeline[WB].valid && pipeline[WB].instr.opcode == HALT) {
            // This break only triggers if exit(0) doesn't work.
            break;
        }

        if (!active_instructions_remaining && (pipeline_halt_seen || pipeline_pc >= (MAX_MEMORY_LINES * WORD_SIZE))) {
            // DEBUG Statement
             printf("Program finished (pipeline drained).\n");
             break;
        }

        if (clock_cycles > 100000) {
            fprintf(stderr, "Simulator possibly in infinite loop, breaking.\n");
            break;
        }
    }
    print_final_state(); // Print final state of registers and memory
} */
