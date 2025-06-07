/*
* Pipeline Simulation with No Forwarding
* This file implements a simple pipeline simulation without forwarding.
* It simulates a 5-stage pipeline with instruction fetch, decode, execute,
* memory access, and write-back stages.
* The pipeline handles hazards, stalls, and flushing as needed.
* It also includes debugging statements to trace the execution flow.
* The pipeline depth is set to 5 stages.
* The simulation stops when a HALT instruction is encountered.
* The pipeline is initialized with NOP instructions.
* The simulation tracks clock cycles, stalls, and flushes.
* The pipeline uses a circular buffer to hold the pipeline registers.
*
* Supported Operations:
* - Instruction Fetch (IF)
* - Instruction Decode (ID)
* - Execute (EX)
* - Memory Access (MEM)
* - Write Back (WB)
* - Hazard Detection (RAW)
* - Branch Resolution
* - NOP Insertion
* - HALT Handling
* - Debugging Output
*
* Functions:
* - is_nop: Checks if an instruction is a NOP.
* - insert_nop: Inserts a NOP instruction into a specified pipeline stage.
* - initialize_pipeline: Initializes the pipeline with NOPs.
* - get_dest_reg: Returns the destination register for an instruction.
* - is_source_reg: Checks if a register is a source register for an instruction.
* - detect_raw_hazard: Detects RAW hazards in the pipeline.
* - simulate_one_cycle_no_forwarding_internal: Simulates one clock cycle of the pipeline.
* - simulate_pipeline_no_forwarding: Main function to run the pipeline simulation.
*
*/

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


/*
* Function to check if an instruction is a NOP (No Operation)
* This function checks if the opcode of the instruction is NOP.
* It returns 1 if the instruction is a NOP, otherwise returns 0.
*/
int is_nop(DecodedInstruction instr) {
    return instr.opcode == NOP;
}

/*
* Function to insert a NOP instruction into a specified pipeline stage
* This function sets the instruction in the specified stage to NOP,
* marks it as invalid, clears the PC, and resets branch flags.
* It is used to handle stalls or flushes in the pipeline.
*/
void insert_nop(int stage, PipelineRegister pipeline_arr[]) {
    pipeline_arr[stage].instr = NOP_INSTRUCTION;
    pipeline_arr[stage].valid = 0; // Mark as invalid (NOP)
    pipeline_arr[stage].pc = 0; // Clear PC for NOPs
    pipeline_arr[stage].branch_taken = 0; // Clear branch flags
    pipeline_arr[stage].branch_target = 0;
    pipeline_arr[stage].result_val = 0; // Clear result
}

/*
* Function to initialize the pipeline with NOP instructions
* This function fills the pipeline with NOP instructions for each stage.
* It sets the pipeline PC to 0, resets the halt flag, and initializes
* the clock cycle count and instruction counters.
*/
void initialize_pipeline(PipelineRegister pipeline_arr[]) {
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        insert_nop(i, pipeline_arr);
    }
    pipeline_pc = 0; // Start fetching from address 0
    pipeline_halt_seen = 0;
    clock_cycles = 0;
    total_stalls = 0;
    total_flushes = 0;
    total_instructions = 0; // Reset functional sim's instruction counters
    arithmetic_instructions = 0;
    logical_instructions = 0;
    memory_access_instructions = 0;
    control_transfer_instructions = 0;
}

/*
* Function to get the destination register for an instruction
* This function returns the destination register number for a given instruction.
* For R-type instructions, it returns the rd field.
* For I-type instructions, it returns the rt field for ADDI, SUBI, MULI, ORI, ANDI, XORI, and LDW.
* If the instruction does not have a destination register, it returns -1.
* This is used to check for RAW hazards in the pipeline.
*/
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

/*
* Function to check if a register is a source register for an instruction
* This function checks if a given register number is a source register for the instruction.
* It returns 1 if the register is a source register, otherwise returns 0.
* For R-type instructions, it checks if the register is either rs or rt.
* For I-type instructions, it checks based on the opcode:
* - BZ: checks rs
* - BEQ: checks rs and rt
* - JR: checks rs
* - STW: checks rs and rt (rt is the value to store)
* - Other I-type instructions: checks rs and rt (rt is the value to store)
* Note: R0 is not considered a source register.
* This is used to detect RAW hazards in the pipeline.
*/
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

/*
* Function to detect RAW hazards in the pipeline
* This function checks for RAW hazards between the current ID stage instruction
* and the EX and MEM stage instructions.
* It returns 1 if a RAW hazard is detected, otherwise returns 0.
* It checks if the source registers of the current ID instruction match
* the destination registers of the EX and MEM stage instructions.
* It considers R0 as not a hazard source/destination.
* It also handles STW instructions, which use rt as a source register (value to store).
* The function uses the get_dest_reg function to get the destination registers
* of the EX and MEM stage instructions.
* The function also checks if the current ID instruction is valid and not a NOP or HALT.
* If a hazard is detected, it returns 1, indicating a stall is needed.
* If no hazard is detected, it returns 0.
* This function is called during the ID stage of the pipeline to check for hazards.
* It is used to determine if the pipeline needs to stall due to a RAW hazard.
*/
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

/*
* Function to simulate one cycle of the pipeline without forwarding
* This function simulates one clock cycle of the pipeline without forwarding.
* It handles instruction fetch, decode, execute, memory access, and write-back stages.
* It also detects hazards, stalls, and flushes as needed.
* The function updates the pipeline registers and the program counter (PC).
* It also prints debugging information about the pipeline state and instruction execution.
* The function is called repeatedly to simulate the pipeline until a HALT instruction is encountered.
* It uses a circular buffer to hold the pipeline registers.
* The function also handles branch resolution and updates the PC accordingly.
* It checks for RAW hazards in the ID stage and stalls the pipeline if necessary.
* The function also handles flushing the pipeline if a branch is taken.
* It prints debugging statements to trace the execution flow and state of the pipeline.
* The function is designed to be called in a loop until the pipeline is empty or a HALT instruction is processed.
* It also includes debugging statements to trace the execution flow and state of the pipeline.
* The function is called by the main simulation loop in simulate_pipeline_no_forwarding.
* It is responsible for managing the pipeline stages and ensuring correct execution of instructions.
*/
void simulate_one_cycle_no_forwarding_internal() {
    clock_cycles++;

    // Debugging Statements
    // --- Optional: Print header for the current cycle ---
    DBG_PRINTF("Clock cycle: %d\n", clock_cycles);
    DBG_PRINTF("  Reg State: R1=%d, R2=%d, R3=%d, R4=%d, R5=%d, R6=%d, R7=%d, R8=%d, R9=%d, R10=%d, R11=%d, R12=%d, R13=%d, R14=%d, R15=%d\n",
           state.registers[1], state.registers[2], state.registers[3], state.registers[4], state.registers[5],
           state.registers[6], state.registers[7], state.registers[8], state.registers[9], state.registers[10],
           state.registers[11], state.registers[12], state.registers[13], state.registers[14], state.registers[15]);

    DBG_PRINTF("Pipeline state: IF=%s, ID=%s, EX=%s, MEM=%s, WB=%s\n",
           opcode_to_string(pipeline[IF].instr.opcode), opcode_to_string(pipeline[ID].instr.opcode),
           opcode_to_string(pipeline[EX].instr.opcode), opcode_to_string(pipeline[MEM].instr.opcode),
           opcode_to_string(pipeline[WB].instr.opcode));
    DBG_PRINTF("Pipeline PC: %u\n", pipeline_pc);

    // Debugging PC at 88
    if (pipeline[WB].valid && pipeline[WB].pc == 88) {
        DBG_PRINTF("DEBUG WB (Cycle %d): PC=%u, Opcode in pipeline[WB].instr = 0x%X, Expected STW (0x0D)\n",
            clock_cycles, pipeline[WB].pc, pipeline[WB].instr.opcode);

    }

    // --- Stage Execution (in reverse order for pipeline integrity) ---
    // 1. WB stage execution: Instructions commit and update architectural state
    if (pipeline[WB].valid && !is_nop(pipeline[WB].instr)) {
        if (pipeline[WB].instr.opcode == HALT) {
            DBG_PRINTF("[PIPE_DEBUG] HALT in WB. pipeline[WB].pc = 0x%X. Current state.pc BEFORE assignment = 0x%X\n", 
                   pipeline[WB].pc, state.pc);
        }

        state.pc = pipeline[WB].pc;

        if (pipeline[WB].instr.opcode == HALT) {
            DBG_PRINTF("[PIPE_DEBUG] HALT in WB. state.pc AFTER assignment (entry to simulate_instruction) = 0x%X\n", 
                   state.pc);
        }

        simulate_instruction(pipeline[WB].instr);
    } 

    int raw_hazard_stall_this_cycle = 0;
    int branch_flush_this_cycle = 0;

    // 2. Branch Resolution in EX stage
    if (pipeline[EX].valid && !is_nop(pipeline[EX].instr) &&
        (pipeline[EX].instr.opcode == BEQ || pipeline[EX].instr.opcode == BZ || pipeline[EX].instr.opcode == JR)) {

        // DEBUG Statement
        DBG_PRINTF("  Branch check in EX: PC=%u, Opcode=%d\n", pipeline[EX].pc, pipeline[EX].instr.opcode);

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
            DBG_PRINTF("Branch taken in EX stage. Flushing IF and ID. New PC: %u\n", pipeline_pc);
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
            DBG_PRINTF("RAW hazard detected. Stalling pipeline.\n");
            total_stalls++;
        }
    }

    // 4. Advance/Stall pipeline stages
    pipeline[WB] = pipeline[MEM];

    if (raw_hazard_stall_this_cycle) {
        pipeline[MEM] = pipeline[EX];
        insert_nop(EX, pipeline);
        // DEBUG Statement
        DBG_PRINTF("Pipeline stalled. Inserting NOP into EX stage.\n");
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
            DBG_PRINTF("HALT instruction fetched. Stopping further fetches.\n");
            pipeline_halt_seen = 1;
        }

        pipeline_pc += WORD_SIZE;

        // DEBUG Statement
        DBG_PRINTF("Fetched instruction at PC: %u. Opcode: %d\n", pipeline[IF].pc, fetched.opcode);
    } else if (!raw_hazard_stall_this_cycle && pipeline_halt_seen) {
        insert_nop(IF, pipeline);
        // DEBUG Statement
        DBG_PRINTF("Inserting NOP into IF stage because HALT was previously fetched and no stall/flush.\n");
    } else if (!raw_hazard_stall_this_cycle && !pipeline_halt_seen && pipeline_pc >= (MAX_MEMORY_LINES * WORD_SIZE)) {
        insert_nop(IF, pipeline);
        // DEBUG Statement
        DBG_PRINTF("Inserting NOP into IF stage because PC (%u) is out of memory bounds, effectively halting.\n", pipeline_pc);
        pipeline_halt_seen = 1;
    }
}

/*
* Function to simulate the pipeline without forwarding
* This function runs the pipeline simulation without forwarding.
* It initializes the pipeline, simulates one cycle at a time,
* and checks for HALT instructions to stop the simulation.
* It also tracks the number of instructions executed and handles
* the final state of the pipeline.
* The function continues to simulate until a HALT instruction is processed
* or the pipeline is empty.
* It prints the final state of the pipeline and the architectural state.
* The function is designed to be called by the main simulation loop.
* It handles the entire pipeline execution flow, including instruction fetch,
* decode, execute, memory access, and write-back stages.
* It also includes debugging statements to trace the execution flow and state of the pipeline.
* The function is responsible for managing the pipeline stages and ensuring correct execution of instructions.
* It also handles stalls, flushes, and hazard detection as needed.
* The function is called by the main simulation loop in functional_sim.c.
* It is the main entry point for running the pipeline simulation without forwarding.
*/
void simulate_pipeline_no_forwarding() {
    initialize_pipeline(pipeline);
    int final_halt_processed_in_wb = 0; // Flag

    while (1) {
        simulate_one_cycle_no_forwarding_internal(); 

        if (pipeline[WB].valid && pipeline[WB].instr.opcode == HALT) {

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

    print_final_state(); 
    // Note: Not iterating PC by 4 again, or instruction counts by 1 again.
}
