/*
* Pipeline simulator with forwarding support.
* This file implements a pipeline simulator that supports forwarding
* to resolve data hazards, particularly Load-Use hazards.
* It simulates a 5-stage pipeline with forwarding paths from EX and MEM stages.
* 
* Supported Operations:
* - R-Type: ADD, SUB, MUL, OR, AND, XOR
* - I-Type: ADDI, SUBI, MULI, ORI, ANDI, XORI, LDW, STW
* - Control: BZ, BEQ, JR, HALT
*
* Functions:
* - initialize_pipeline_fwd: Initializes the pipeline registers to NOPs.
* - simulate_pipeline_with_forwarding: Main simulation loop that processes instructions.
* - detect_raw_hazard_with_fwd: Detects RAW hazards and returns if a stall is needed.
* - simulate_one_cycle_with_forwarding_internal: Simulates one cycle of the pipeline with forwarding.
* - instr_writes_to_reg: Checks if an instruction writes to a register.
*
*/

#include "with_fwd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "functional_sim.h"
#include "instruction_decoder.h"
#include "no_fwd.h"        // For PipelineRegister struct, pipeline_stages enum, NOP_INSTRUCTION
#include "trace_reader.h"  // For MAX_MEMORY_LINES and WORD_SIZE

#define PIPELINE_DEPTH 5

// Global counters (these must be extern if defined in global_counters.c)
extern int clock_cycles;
extern int total_stalls;
extern int total_flushes;

// Global NOP_INSTRUCTION (from global_counters.c)
extern DecodedInstruction NOP_INSTRUCTION;

// Pipeline stages (conceptual pipeline registers) - MAKE STATIC
static PipelineRegister pipeline[PIPELINE_DEPTH];

// Internal PC for pipeline fetch - MAKE STATIC
static uint32_t pipeline_pc = 0;
static int pipeline_halt_seen = 0;

/*
* Initializes the pipeline registers to NOPs.
* This function sets all pipeline registers to NOP_INSTRUCTION,
* which is defined in global_counters.c.
*/
void initialize_pipeline_fwd() {    // Renamed to avoid collision with no_fwd.c for main init
    initialize_pipeline(pipeline);  // Use the common initialization function
    // Specific resets for this simulator if needed, but common init handles all.
}

// Additional global variables for forwarding.
static int forward_src1_from_ex_output_to_ex_input = 0;  // For EX -> EX forwarding path
static int32_t result_for_src1_from_ex_output = 0;
static int forward_src2_from_ex_output_to_ex_input = 0;
static int32_t result_for_src2_from_ex_output = 0;

static int forward_src1_from_mem_output_to_ex_input = 0;  // For MEM -> EX forwarding path
static int32_t result_for_src1_from_mem_output = 0;
static int forward_src2_from_mem_output_to_ex_input = 0;
static int32_t result_for_src2_from_mem_output = 0;

static int stall_for_load_use = 0;

/*
* Checks if an instruction writes to a register.
* This helper function checks if the given instruction writes to a register.
* It returns 1 if it writes to a register, 0 otherwise.
*/
int instr_writes_to_reg(DecodedInstruction instr) {
    if (instr.type == R_TYPE && instr.rd != 0) return 1;
    if (instr.type == I_TYPE && instr.rt != 0) {
        switch (instr.opcode) {
            case ADDI:
            case SUBI:
            case MULI:
            case ORI:
            case ANDI:
            case XORI:
            case LDW:
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}

/*
* Detects RAW hazards with forwarding.
* This function checks if the current instruction in ID stage has a RAW hazard
* with the instruction in EX or MEM stages, considering forwarding paths.
* It returns 1 if a hazard is detected (requiring a stall), 0 otherwise.
*/
int detect_raw_hazard_with_fwd(PipelineRegister curr_id_reg, PipelineRegister ex_reg, PipelineRegister mem_reg) {
    if (!curr_id_reg.valid || is_nop(curr_id_reg.instr) || curr_id_reg.instr.opcode == HALT) return 0;

    DecodedInstruction curr_id = curr_id_reg.instr;
    int src1 = curr_id.rs;
    int src2 = -1;

    if (curr_id.type == R_TYPE || curr_id.opcode == BEQ) {
        src2 = curr_id.rt;
    }
    if (src1 == 0) src1 = -1;
    if (src2 == 0) src2 = -1;

    // Load-Use Hazard: LDW in EX stage, consumer in ID stage
    if (ex_reg.valid && ex_reg.instr.opcode == LDW) {
        int ex_dest = get_dest_reg(ex_reg.instr);  // Use common get_dest_reg
        if (ex_dest != -1) {
            if ((src1 != -1 && src1 == ex_dest) || (src2 != -1 && src2 == ex_dest)) {
                return 1;  // Load-Use hazard detected, needs 1 stall cycle
            }
        }
    }

    return 0;
}

/* 
* Simulates one cycle of the pipeline with forwarding.
* This function simulates one cycle of the pipeline, handling all stages:
* - Write-Back (WB): Writes results to the register file.
* - Memory Access (MEM): Handles memory reads/writes and passes results to WB.
* - Execute (EX): Performs ALU operations, calculates effective addresses, and handles branches.
* - Instruction Decode (ID): Decodes instructions and checks for hazards.
* - Instruction Fetch (IF): Fetches the next instruction from memory.
* It also handles forwarding paths from EX and MEM stages to EX inputs.
* It updates the pipeline registers and handles stalls and flushes as needed.
*/
static void simulate_one_cycle_with_forwarding_internal() {
    clock_cycles++;

    // Reset forwarding signals for the current cycle's detection phase
    forward_src1_from_ex_output_to_ex_input = 0;
    forward_src2_from_ex_output_to_ex_input = 0;
    forward_src1_from_mem_output_to_ex_input = 0;
    forward_src2_from_mem_output_to_ex_input = 0;
    // forward_src1_from_wb_output_to_ex_input = 0; // If using WB->EX
    // forward_src2_from_wb_output_to_ex_input = 0; // If using WB->EX
    int stall_for_load_use = 0;
    int flush_for_branch = 0;

    int current_cycle_branch_taken_in_ex = 0;
    uint32_t current_cycle_branch_target_pc = 0;

    // --- WB (Write-Back) Stage ---
    // Writes pipeline[WB].result_val to register file.
    // Calls simulate_instruction for PC update and counting.
    if (pipeline[WB].valid && !is_nop(pipeline[WB].instr)) {
        simulate_instruction(pipeline[WB].instr);
        state.pc = pipeline[WB].pc;  // Update PC to the one in WB stage
    }

    // --- MEM (Memory Access) Stage ---
    // For LDW: reads memory, result goes to pipeline[MEM].result_val (for WB next cycle).
    // For STW: writes data (from pipeline[EX].result_val via pipeline[MEM].result_val) to memory.
    // For ALU: pipeline[MEM].result_val gets pipeline[EX].result_val.
    // It also passes through branch information from EX.
    if (pipeline[MEM].valid && !is_nop(pipeline[MEM].instr)) {
        DecodedInstruction mem_instr = pipeline[MEM].instr;
        uint32_t eff_addr = pipeline[MEM].branch_target;  // Address came from EX's branch_target field

        if (mem_instr.opcode == LDW) {
            if (eff_addr < MAX_MEMORY_LINES * 4 && (eff_addr % 4 == 0)) {
                pipeline[MEM].result_val = state.memory[eff_addr / 4];
            } else {
                pipeline[MEM].result_val = 0;  // Handle error
            }
        } else if (mem_instr.opcode == STW) {
            // Data for STW was in pipeline[MEM].result_val (passed from EX's result_val)
            if (eff_addr < MAX_MEMORY_LINES * 4 && (eff_addr % 4 == 0)) {
                state.memory[eff_addr / 4] = pipeline[MEM].result_val;
                if (memory_changed) memory_changed[eff_addr / 4] = 1;
            } else {
                // fprintf(stderr, "MEM Error: Bad address 0x%X for STW@0x%X\n", eff_addr, pipeline[MEM].pc);
            }
            // STW does not update result_val for register WB, but it used result_val for data.
        }
        // For ALU ops, pipeline[MEM].result_val already holds the value from EX.
        // Branch info is already in pipeline[MEM].branch_taken and .branch_target from EX.
    }

    // --- EX (Execute / Address Calculation) Stage ---
    // Also clear these bits (Edit 06/04/2025 at 11:40 pm)
    pipeline[EX].branch_taken = 0;
    pipeline[MEM].branch_taken = 0;
    pipeline[WB].branch_taken = 0;

    if (pipeline[EX].valid && !is_nop(pipeline[EX].instr)) {
        DecodedInstruction instr_ex = pipeline[EX].instr;
        int32_t val_rs = state.registers[instr_ex.rs];  // Default from register file
        int32_t val_rt = state.registers[instr_ex.rt];  // Default from register file
        int32_t ex_stage_output_value = 0;

        // ** REVISED FORWARDING LOGIC FOR EX STAGE OPERANDS **
        // Order of checks: MEM stage output (1 cycle ahead), then WB stage output (2 cycles ahead)

        // Forwarding for Rs (Operand 1 from instr_ex.rs)
        int rs_forwarded = 0;
        if (pipeline[MEM].valid && instr_writes_to_reg(pipeline[MEM].instr) &&
            get_dest_reg(pipeline[MEM].instr) == instr_ex.rs && instr_ex.rs != 0) {
            val_rs = pipeline[MEM].result_val;  // Forward from MEM stage output
            rs_forwarded = 1;
            // printf("Cycle %d: EX_PC=0x%X fwd Rs from MEM_PC=0x%X (val=%d)\n", clock_cycles, pipeline[EX].pc, pipeline[MEM].pc, val_rs);
        }
        if (!rs_forwarded && pipeline[WB].valid && instr_writes_to_reg(pipeline[WB].instr) &&
            get_dest_reg(pipeline[WB].instr) == instr_ex.rs && instr_ex.rs != 0) {
            val_rs = pipeline[WB].result_val;  // Forward from WB stage output
            // printf("Cycle %d: EX_PC=0x%X fwd Rs from WB_PC=0x%X (val=%d)\n", clock_cycles, pipeline[EX].pc, pipeline[WB].pc, val_rs);
        }

        // Forwarding for Rt (Operand 2, if Rt is a source for instr_ex)
        if (instr_ex.type == R_TYPE || instr_ex.opcode == BEQ || instr_ex.opcode == STW) {
            int rt_forwarded = 0;
            if (pipeline[MEM].valid && instr_writes_to_reg(pipeline[MEM].instr) &&
                get_dest_reg(pipeline[MEM].instr) == instr_ex.rt && instr_ex.rt != 0) {
                val_rt = pipeline[MEM].result_val;
                rt_forwarded = 1;
                // printf("Cycle %d: EX_PC=0x%X fwd Rt from MEM_PC=0x%X (val=%d)\n", clock_cycles, pipeline[EX].pc, pipeline[MEM].pc, val_rt);
            }
            if (!rt_forwarded && pipeline[WB].valid && instr_writes_to_reg(pipeline[WB].instr) &&
                get_dest_reg(pipeline[WB].instr) == instr_ex.rt && instr_ex.rt != 0) {
                val_rt = pipeline[WB].result_val;
                // printf("Cycle %d: EX_PC=0x%X fwd Rt from WB_PC=0x%X (val=%d)\n", clock_cycles, pipeline[EX].pc, pipeline[WB].pc, val_rt);
            }
        }

        // Execute operation using (potentially forwarded) val_rs, val_rt
        uint32_t current_ex_pc = pipeline[EX].pc;

        switch (instr_ex.opcode) {
            // ALU R-Type
            case ADD:
                ex_stage_output_value = val_rs + val_rt;
                break;
            case SUB:
                ex_stage_output_value = val_rs - val_rt;
                break;
            // ... (Include ALL your ALU R-Type and I-Type cases here) ...
            case MUL:
                ex_stage_output_value = val_rs * val_rt;
                break;
            case OR:
                ex_stage_output_value = val_rs | val_rt;
                break;
            case AND:
                ex_stage_output_value = val_rs & val_rt;
                break;
            case XOR:
                ex_stage_output_value = val_rs ^ val_rt;
                break;
            case ADDI:
                ex_stage_output_value = val_rs + instr_ex.immediate;
                break;
            case SUBI:
                ex_stage_output_value = val_rs - instr_ex.immediate;
                break;
            case MULI:
                ex_stage_output_value = val_rs * instr_ex.immediate;
                break;
            case ORI:
                ex_stage_output_value = val_rs | (instr_ex.immediate & 0xFFFF);
                break;
            case ANDI:
                ex_stage_output_value = val_rs & (instr_ex.immediate & 0xFFFF);
                break;
            case XORI:
                ex_stage_output_value = val_rs ^ (instr_ex.immediate & 0xFFFF);
                break;

            case LDW:
                pipeline[EX].branch_target = val_rs + instr_ex.immediate;  // Store effective_address
                ex_stage_output_value = 0;                                 // Actual data loaded in MEM stage for LDW
                break;
            case STW:
                pipeline[EX].branch_target = val_rs + instr_ex.immediate;  // Store effective_address
                ex_stage_output_value = val_rt;                            // Pass data_to_store (from val_rt) via result_val
                break;

            // Control
            case BZ:
                if (val_rs == 0) {
                    pipeline[EX].branch_taken = 1;
                    pipeline[EX].branch_target = current_ex_pc + (instr_ex.immediate * 4);  // Corrected target
                }
                break;
            case BEQ:
                // DEBUG (06/04/2025 at 11:43 PM):
                DBG_PRINTF(stderr, "DEBUG: BEQ at EX PC=0x%X, val_rs=%d, val_rt=%d, imm=%d\n",
                        current_ex_pc, val_rs, val_rt, instr_ex.immediate);
                if (val_rs == val_rt) {  // This comparison needs correct val_rs & val_rt
                    pipeline[EX].branch_taken = 1;
                    pipeline[EX].branch_target = current_ex_pc + (instr_ex.immediate * 4);  // Corrected target
                }
                // Debug BEQ decision:
                DBG_PRINTF("Cycle %d: EX BEQ PC=0x%X, R10(val_rs)=%d, R11(val_rt)=%d, Taken=%d\n",
                        clock_cycles, current_ex_pc, val_rs, val_rt, pipeline[EX].branch_taken);
                break;
            case JR:
                pipeline[EX].branch_taken = 1;
                pipeline[EX].branch_target = (uint32_t)val_rs;
                break;

            case HALT:
            case NOP:
                ex_stage_output_value = 0;
                break;
            default:
                ex_stage_output_value = 0;
                break;
        }
        pipeline[EX].result_val = ex_stage_output_value;  // Store result for MEM stage / forwarding

        if (pipeline[EX].branch_taken) {
            pipeline_pc = pipeline[EX].branch_target;  // Update global fetch PC
            flush_for_branch = 1;                      // Ensure this is declared or is your global flag
            total_flushes += 2;
        }
    } else {
        pipeline[EX].result_val = 0;
    }

    // --- ID Stage: Decode & Stall for Load-Use Hazard ---
    // Load-use: LDW currently in EX, and instruction currently in ID needs its result.
    // The result of LDW in EX will be available from MEM stage output *next cycle*.
    // The instruction in ID will be in EX *next cycle*. So it needs to stall for one cycle.
    stall_for_load_use = 0;
    if (pipeline[EX].valid && pipeline[EX].instr.opcode == LDW && instr_writes_to_reg(pipeline[EX].instr)) {
        int dest_reg_of_load_in_ex = get_dest_reg(pipeline[EX].instr);  // This is pipeline[EX].instr.rt
        if (dest_reg_of_load_in_ex != 0) {
            if (pipeline[ID].valid && !is_nop(pipeline[ID].instr)) {
                DecodedInstruction id_instr = pipeline[ID].instr;
                int id_needs_rs = (id_instr.rs == dest_reg_of_load_in_ex);
                int id_needs_rt_as_src = ((id_instr.type == R_TYPE || id_instr.opcode == BEQ || id_instr.opcode == STW) &&
                                          (id_instr.rt == dest_reg_of_load_in_ex));
                if (id_needs_rs || id_needs_rt_as_src) {
                    stall_for_load_use = 1;
                }
            }
        }
    }
    if (stall_for_load_use) {
        total_stalls++;
    }

    // --- Pipeline Stage Advancement (Shift Registers) ---
    // Order matters: WB gets old MEM, MEM gets old EX, etc.
    pipeline[WB] = pipeline[MEM];
    pipeline[MEM] = pipeline[EX];

    // ─── Decide what goes into EX next ─────────────────────────────
    if (stall_for_load_use) {
        // We have to insert a bubble (NOP) in EX and keep ID as-is
        insert_nop(EX, pipeline);
    } else if (flush_for_branch) {
        // Branch was taken in EX: forcibly squash the next instruction in EX
        insert_nop(EX, pipeline);
    } else {
        // Normal pipeline advance
        pipeline[EX] = pipeline[ID];
    }

    // ─── Now handle flushing ID/IF ─────────────────────────────────
    if (flush_for_branch) {
        // We already forced EX = NOP above. Now squash ID and IF.
        insert_nop(ID, pipeline);
        insert_nop(IF, pipeline);
        flush_for_branch = 0;  // done handling the flush
    } else if (stall_for_load_use) {
        // Do nothing: keep ID/IF as they were (so ID still holds the consumer waiting on load)
    } else {
        // Normal, no-stall, no-branch: push IF→ID, then IF = NOP
        pipeline[ID] = pipeline[IF];
        insert_nop(IF, pipeline);
    }
    // --- IF (Instruction Fetch) Stage ---
    if (stall_for_load_use) {
        // IF stage is stalled, pipeline_pc does not advance. pipeline[IF] holds its current instruction.
    } else if (flush_for_branch) {
        // IF stage has been NOPped above. pipeline_pc is already pointing to branch target.
        // Fetch will happen from new PC in the next cycle's IF stage.
    } else {  // Not stalling for load-use, not flushing this cycle
        if (!pipeline_halt_seen && pipeline_pc < (MAX_MEMORY_LINES * WORD_SIZE)) {
            uint32_t instr_word_fetched = state.memory[pipeline_pc / 4];
            DecodedInstruction fetched = decode_instruction(instr_word_fetched);
            pipeline[IF].instr = fetched;
            pipeline[IF].valid = 1;
            pipeline[IF].pc = pipeline_pc;
            if (fetched.opcode == HALT) {
                pipeline_halt_seen = 1;
            }
            pipeline_pc += 4;  // Advance fetch PC for next instruction
        } else {
            insert_nop(IF, pipeline);  // PC out of bounds or halt already seen
            if (pipeline_pc >= (MAX_MEMORY_LINES * WORD_SIZE) && !pipeline_halt_seen) {
                pipeline_halt_seen = 1;
            }
        }
    }
}

/*
* Simulates the pipeline with forwarding support.
* This function initializes the pipeline and runs the simulation loop.
* It processes instructions in the pipeline, handling stalls and flushes as needed.
* It continues until a HALT instruction is encountered or no active instructions remain.
*/
void simulate_pipeline_with_forwarding() {
    initialize_pipeline_fwd();

    while (1) {
        int active_instructions_remaining = 0;
        for (int i = 0; i < PIPELINE_DEPTH; i++) {
            if (pipeline[i].valid && pipeline[i].instr.opcode != NOP) {
                active_instructions_remaining = 1;
                break;
            }
        }

        if (pipeline[WB].valid && pipeline[WB].instr.opcode == HALT) {
            // 1) Retire HALT (this will bump all counters and advance PC by 4 inside simulate_instruction)
            simulate_instruction(pipeline[WB].instr);
            // 2) Now stop the pipeline loop
            break;
        }

        if (!active_instructions_remaining && (pipeline_halt_seen || pipeline_pc >= (MAX_MEMORY_LINES * WORD_SIZE))) {
            break;
        }

        simulate_one_cycle_with_forwarding_internal();

        if (clock_cycles > 100000) {
            fprintf(stderr, "Simulator possibly in infinite loop, breaking.\n");
            break;
        }
    }
    // Fix state PC stuff
    state.pc += 4;

    print_final_state();  // Print final state after simulation ends
}
