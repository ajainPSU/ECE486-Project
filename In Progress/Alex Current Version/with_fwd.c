// with_fwd.c (Modified)
/*
* Alex Jain - 06/03/2025
* ECE 486 / Pipeline Simulator (With Forwarding)
* Implements a 5-stage pipeline with RAW hazard detection and forwarding,
* and branch prediction (always not taken) with flushing.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "with_fwd.h"
#include "instruction_decoder.h"
#include "functional_sim.h"
#include "no_fwd.h"         // For PipelineRegister struct, pipeline_stages enum, NOP_INSTRUCTION
#include "trace_reader.h"   // For MAX_MEMORY_LINES and WORD_SIZE

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

// Helper function to insert a NOP into a pipeline stage
// Using the common insert_nop from no_fwd.h
// void insert_nop_fwd(int stage, PipelineRegister pipeline_arr[]); // Removed, using common one

// Initialize pipeline to NOPs
void initialize_pipeline_fwd() { // Renamed to avoid collision with no_fwd.c for main init
    initialize_pipeline(pipeline); // Use the common initialization function
    // Specific resets for this simulator if needed, but common init handles all.
}

// Reusing get_dest_reg and is_source_reg from no_fwd.c (assuming they are generally accessible
// or you copied them directly to instruction_decoder.c / instruction_decoder.h)
// If not, declare them here as static or copy their implementations.
// For now, assuming they are accessible via instruction_decoder.h or no_fwd.h includes.
// If your compiler complains, copy them here or declare them extern where they are defined.


// Detect RAW hazard for a pipeline WITH forwarding.
// Only Load-Use hazard causes a stall.
// Returns 1 if a hazard is detected (and a stall is needed), 0 otherwise.
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
        int ex_dest = get_dest_reg(ex_reg.instr); // Use common get_dest_reg
        if (ex_dest != -1) {
            if ((src1 != -1 && src1 == ex_dest) || (src2 != -1 && src2 == ex_dest)) {
                return 1; // Load-Use hazard detected, needs 1 stall cycle
            }
        }
    }

    return 0;
}

// Simulate one clock cycle of the pipeline (With Forwarding)
void simulate_one_cycle_with_forwarding() {
    clock_cycles++;
    // --- Stage Execution (in reverse order for pipeline integrity) ---
    // 1. WB stage: Write back results to registers (final commit)
    if (pipeline[WB].valid && !is_nop(pipeline[WB].instr)) {
        DecodedInstruction instr = pipeline[WB].instr;
        if (instr.opcode == HALT) {
            simulate_instruction(instr); // This will call exit(0)
            return;
        }
        int dest_reg = get_dest_reg(instr);
        if (dest_reg != -1) {
            state.registers[dest_reg] = pipeline[WB].result_val;
            register_written[dest_reg] = 1;
        }

        if (instr.opcode != NOP) {
            total_instructions++;
            if (instr.type == R_TYPE || instr.opcode == ADDI || instr.opcode == SUBI || instr.opcode == MULI) {
                arithmetic_instructions++;
            } else if (instr.opcode == OR || instr.opcode == ORI || instr.opcode == AND || instr.opcode == ANDI || instr.opcode == XOR || instr.opcode == XORI) {
                logical_instructions++;
            } else if (instr.opcode == LDW || instr.opcode == STW) {
                memory_access_instructions++;
            } else if (instr.opcode == BZ || instr.opcode == BEQ || instr.opcode == JR || instr.opcode == HALT) {
                control_transfer_instructions++;
            }
        }
    }

    // 2. MEM stage: Memory access (LDW/STW)
    pipeline[WB] = pipeline[MEM];

    if (pipeline[EX].valid && !is_nop(pipeline[EX].instr)) {
        DecodedInstruction instr = pipeline[EX].instr;
        if (instr.opcode == LDW) {
            int32_t address = state.registers[instr.rs] + instr.immediate;
            if (address % 4 != 0) {
                fprintf(stderr, "Error: Unaligned memory access at address 0x%X for LDW in MEM stage\n", address);
            }
            pipeline[EX].result_val = state.memory[address / 4];
        } else if (instr.opcode == STW) {
            int32_t address = state.registers[instr.rs] + instr.immediate;
            if (address % 4 != 0) {
                fprintf(stderr, "Error: Unaligned memory access at address 0x%X for STW in MEM stage\n", address);
            }
            state.memory[address / 4] = state.registers[instr.rt];
            memory_changed[address / 4] = 1;
        }
    }
    pipeline[MEM] = pipeline[EX];

    // 3. EX stage: Execute (ALU ops, branch condition evaluation)
    pipeline[EX] = pipeline[ID];

    if (pipeline[ID].valid && !is_nop(pipeline[ID].instr)) {
        DecodedInstruction instr = pipeline[ID].instr;
        int32_t val_rs = state.registers[instr.rs];
        int32_t val_rt = state.registers[instr.rt];

        // Apply forwarding: Check EX, MEM for producers.
        if (instr.rs != 0) {
            int ex_dest_reg = get_dest_reg(pipeline[EX].instr);
            if (pipeline[EX].valid && ex_dest_reg == instr.rs) {
                 val_rs = pipeline[EX].result_val;
            } else {
                int mem_dest_reg = get_dest_reg(pipeline[MEM].instr);
                if (pipeline[MEM].valid && mem_dest_reg == instr.rs) {
                    val_rs = pipeline[MEM].result_val;
                }
            }
        }
        if (instr.rt != 0 && (instr.type == R_TYPE || instr.opcode == BEQ || instr.opcode == STW)) {
            int ex_dest_reg = get_dest_reg(pipeline[EX].instr);
            if (pipeline[EX].valid && ex_dest_reg == instr.rt) {
                val_rt = pipeline[EX].result_val;
            } else {
                int mem_dest_reg = get_dest_reg(pipeline[MEM].instr);
                if (pipeline[MEM].valid && mem_dest_reg == instr.rt) {
                    val_rt = pipeline[MEM].result_val;
                }
            }
        }

        switch (instr.opcode) {
            case ADD: pipeline[ID].result_val = val_rs + val_rt; break;
            case ADDI: pipeline[ID].result_val = val_rs + instr.immediate; break;
            case SUB: pipeline[ID].result_val = val_rs - val_rt; break;
            case SUBI: pipeline[ID].result_val = val_rs - instr.immediate; break;
            case MUL: pipeline[ID].result_val = val_rs * val_rt; break;
            case MULI: pipeline[ID].result_val = val_rs * instr.immediate; break;
            case OR: pipeline[ID].result_val = val_rs | val_rt; break;
            case ORI: pipeline[ID].result_val = val_rs | instr.immediate; break;
            case AND: pipeline[ID].result_val = val_rs & val_rt; break;
            case ANDI: pipeline[ID].result_val = val_rs & instr.immediate; break;
            case XOR: pipeline[ID].result_val = val_rs ^ val_rt; break;
            case XORI: pipeline[ID].result_val = val_rs ^ instr.immediate; break;
            case BZ:
                if (val_rs == 0) {
                    pipeline[ID].branch_taken = 1;
                    pipeline[ID].branch_target = pipeline[ID].pc + WORD_SIZE + ((int32_t)instr.immediate * WORD_SIZE);
                }
                break;
            case BEQ:
                if (val_rs == val_rt) {
                    pipeline[ID].branch_taken = 1;
                    pipeline[ID].branch_target = pipeline[ID].pc + WORD_SIZE + ((int32_t)instr.immediate * WORD_SIZE);
                }
                break;
            case JR:
                pipeline[ID].branch_taken = 1;
                pipeline[ID].branch_target = (uint32_t)val_rs;
                break;
            case LDW: case STW: case HALT: case NOP:
                break;
            default:
                fprintf(stderr, "Error: Unknown opcode in EX stage: 0x%02X at PC: 0x%08X\n", instr.opcode, pipeline[ID].pc);
                exit(1);
        }
    }

    // 4. ID stage: Decode instruction, detect hazards
    int raw_hazard_stall_this_cycle = 0;
    if (pipeline[ID].valid && !is_nop(pipeline[ID].instr)) {
        raw_hazard_stall_this_cycle = detect_raw_hazard_with_fwd(
            pipeline[ID],
            pipeline[EX],
            pipeline[MEM]
        );

        if (raw_hazard_stall_this_cycle) {
            total_stalls++;
            insert_nop(EX, pipeline);
            return;
        }
    }
    pipeline[ID] = pipeline[IF];


    // 5. IF stage: Fetch instruction
    int branch_flush_this_cycle = 0;
    if (pipeline[EX].valid && pipeline[EX].branch_taken) {
        pipeline_pc = pipeline[EX].branch_target;
        branch_flush_this_cycle = 1;
        total_flushes += 2;
    }

    if (!raw_hazard_stall_this_cycle && !pipeline_halt_seen && pipeline_pc < (MAX_MEMORY_LINES * WORD_SIZE)) {
        uint32_t instr_word = state.memory[pipeline_pc / WORD_SIZE];
        DecodedInstruction fetched = decode_instruction(instr_word);

        pipeline[IF].instr = fetched;
        pipeline[IF].valid = 1;
        pipeline[IF].pc = pipeline_pc;

        if (fetched.opcode == HALT) {
            pipeline_halt_seen = 1;
        }

        pipeline_pc += WORD_SIZE;
    } else {
        if(!raw_hazard_stall_this_cycle) {
            insert_nop(IF, pipeline);
        }
    }

    if (branch_flush_this_cycle) {
        insert_nop(ID, pipeline);
        insert_nop(IF, pipeline);
    }
}

// Main pipeline simulation function with forwarding
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
            break;
        }

        if (!active_instructions_remaining && (pipeline_halt_seen || pipeline_pc >= (MAX_MEMORY_LINES * WORD_SIZE))) {
             break;
        }

        simulate_one_cycle_with_forwarding();

        if (clock_cycles > 100000) {
            fprintf(stderr, "Simulator possibly in infinite loop, breaking.\n");
            break;
        }
    }

    print_final_state(); // Print final state after simulation ends
}