/*
* Alex Jain - 05/30/2025
* ECE 486 / Functional Simulator
* * Takes elements from instruction_decoder.c and trace_reader.c
* Runs a functional simulation of the instruction set architecture.
*
* Version 1.0 - Initial implementation.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "instruction_decoder.h"
#include "functional_sim.h"
#include "trace_reader.h"
#include "no_fwd.h" // For pipeline simulator with no forwarding call.
#include "with_fwd.h" // For pipeline simulator with forwarding call.

// Register Written Tracking and Memory Change Tracking
int register_written[32] = {0};
int memory_changed[1024] = {0}; // Tracks which memory addresses were modified

MachineState state; // Global machine state

// Instruction counters
int total_instructions = 0;
int arithmetic_instructions = 0;
int logical_instructions = 0;
int memory_access_instructions = 0;
int control_transfer_instructions = 0;

// Initialize the machine state
void initialize_machine_state() {
    state.pc = 0; // Initialize PC to 0
    memset(state.registers, 0, sizeof(state.registers)); // Initialize registers to 0
    memset(state.memory, 0, sizeof(state.memory)); // Initialize memory to 0
    memset(register_written, 0, sizeof(register_written)); // Reset register written tracking
    memset(memory_changed, 0, sizeof(memory_changed)); // Reset memory change tracking
    // Note: clock_cycles, total_stalls, total_flushes are in no_fwd.c and initialized there
}

void simulate_instruction(DecodedInstruction instr) {
    // Debug Statement
    printf("SIMULATE_INSTRUCTION called with PC (arch before this instr)=%u, Opcode=0x%X, rs=%d, rt=%d, rd=%d, imm=%d\n",
           state.pc, instr.opcode, instr.rs, instr.rt, instr.rd, instr.immediate);
    // R0 (register 0) is always 0 and cannot be written
    if (instr.type == R_TYPE && instr.rd == 0) {
        // Attempting to write to R0. Do nothing, R0 remains 0.
    } else if (instr.type == I_TYPE && (instr.opcode == ADDI || instr.opcode == SUBI || instr.opcode == MULI || 
                                         instr.opcode == ORI || instr.opcode == ANDI || instr.opcode == XORI || 
                                         instr.opcode == LDW) && instr.rt == 0) {
        // Attempting to write to R0. Do nothing, R0 remains 0.
    } else {
        // Mark destination register as written if it's not R0
        if (instr.type == R_TYPE) {
            register_written[instr.rd] = 1;
        } else if (instr.opcode == ADDI || instr.opcode == SUBI || instr.opcode == MULI ||
                   instr.opcode == ORI || instr.opcode == ANDI || instr.opcode == XORI ||
                   instr.opcode == LDW) {
            register_written[instr.rt] = 1;
        }
    }


    total_instructions++; // Increment total instructions executed by functional sim

    switch (instr.opcode) {
        // Arithmetic Instructions
        case ADD:
            state.registers[instr.rd] = state.registers[instr.rs] + state.registers[instr.rt];
            arithmetic_instructions++;
            break;
        case ADDI:
            state.registers[instr.rt] = state.registers[instr.rs] + instr.immediate;
            arithmetic_instructions++;
            break;
        case SUB:
            state.registers[instr.rd] = state.registers[instr.rs] - state.registers[instr.rt];
            arithmetic_instructions++;
            break;
        case SUBI:
            state.registers[instr.rt] = state.registers[instr.rs] - instr.immediate;
            arithmetic_instructions++;
            break;
        case MUL:
            state.registers[instr.rd] = state.registers[instr.rs] * state.registers[instr.rt];
            arithmetic_instructions++;
            break;
        case MULI:
            state.registers[instr.rt] = state.registers[instr.rs] * instr.immediate;
            arithmetic_instructions++;
            break;

        // Logical Instructions
        case OR:
            state.registers[instr.rd] = state.registers[instr.rs] | state.registers[instr.rt];
            logical_instructions++;
            break;
        case ORI:
            state.registers[instr.rt] = state.registers[instr.rs] | instr.immediate;
            logical_instructions++;
            break;
        case AND:
            state.registers[instr.rd] = state.registers[instr.rs] & state.registers[instr.rt];
            logical_instructions++;
            break;
        case ANDI:
            state.registers[instr.rt] = state.registers[instr.rs] & instr.immediate;
            logical_instructions++;
            break;
        case XOR:
            state.registers[instr.rd] = state.registers[instr.rs] ^ state.registers[instr.rt];
            logical_instructions++;
            break;
        case XORI:
            state.registers[instr.rt] = state.registers[instr.rs] ^ instr.immediate;
            logical_instructions++;
            break;

        // Memory Access Instructions
        case LDW: {
            // Address calculation: R[rs] + immediate (signed offset in bytes)
            int32_t address = state.registers[instr.rs] + instr.immediate;
            // Check for unaligned access (optional, depending on ISA spec)
            if (address % 4 != 0) {
                fprintf(stderr, "Error: Unaligned memory access at address 0x%X for LDW\n", address);
                // Handle error: perhaps exit or ignore, based on project spec
            }
            // Memory is word-addressable in our simulation (address / 4)
            state.registers[instr.rt] = state.memory[address / 4];
            memory_access_instructions++;
            break;
        }
        case STW: {
            // Address calculation: R[rs] + immediate (signed offset in bytes)
            int32_t address = state.registers[instr.rs] + instr.immediate;
            // Check for unaligned access (optional)
            if (address % 4 != 0) {
                fprintf(stderr, "Error: Unaligned memory access at address 0x%X for STW\n", address);
                // Handle error
            }
            state.memory[address / 4] = state.registers[instr.rt];
            memory_changed[address / 4] = 1; // Mark memory as changed
            memory_access_instructions++;
            // Debug statement.
            printf("  EXECUTED STW logic for PC (arch before this instr)=%u. About to break.\n", state.pc); // Use state.pc as it was at entry
            break;
        }

        // Control Transfer Instructions
        case BZ:
            if (state.registers[instr.rs] == 0) {
                state.pc += ((int32_t)instr.immediate) * 4;
                control_transfer_instructions++;
                return; // PC has been updated, so return immediately
            }
            control_transfer_instructions++;
            break; // Fall through if branch not taken
        case BEQ:
            if (state.registers[instr.rs] == state.registers[instr.rt]) {
                state.pc += ((int32_t)instr.immediate) * 4;
                control_transfer_instructions++;
                return; // PC has been updated, so return immediately
            }
            control_transfer_instructions++;
            break; // Fall through if branch not taken
        case JR:
            state.pc = (uint32_t)state.registers[instr.rs]; // Jump to address in Rs
            control_transfer_instructions++;
            return; // PC has been updated, so return immediately
        case HALT:
            control_transfer_instructions++;
            // state.pc += 4; // Increment PC for HALT itself for accurate final PC count
            // Above line removed since it messed up FS's PC counter.
            printf("--- HALT INSTRUCTION PROCESSING ---\n"); // New debug
            printf("--- Architectural PC before this HALT was: %u ---\n", state.pc - 4); // New debug
            printf("--- Architectural PC AFTER HALT increment is: %u ---\n", state.pc); // New debug
            printf("Program halted.\n");
            // print_final_state(); // Print final state upon halt
            // fflush(stdout); // Testing to see if no_fwd.c receives this.
            // exit(0); // Exit the program (removed for break for testing purposes)
            break;;
        case NOP:
            // Do nothing
            break;
        default:
            fprintf(stderr, "Error: Unknown opcode: 0x%02X at PC: 0x%08X\n", instr.opcode, state.pc);
            exit(1); // Exit on unknown opcode
    }

    // Default program counter increment (if not branched or jumped)
    state.pc += 4;
}


// Print the final state of the machine after simulation.
void print_final_state() {
    printf("Functional simulator output is as follows:\n\n");

    // Instruction counts
    printf("Instruction counts:\n");
    printf("Total number of instructions: %d\n", total_instructions);
    printf("Arithmetic instructions: %d\n", arithmetic_instructions);
    printf("Logical instructions: %d\n", logical_instructions);
    printf("Memory access instructions: %d\n", memory_access_instructions);
    printf("Control transfer instructions: %d\n\n", control_transfer_instructions);

    // Final register state
    printf("Final register state:\n");
    printf("Program counter: %u\n", state.pc);
    for (int i = 0; i < 32; i++) {
        // Only print registers that were explicitly written to or are non-zero (optional, but good for debugging)
        if (register_written[i] || state.registers[i] != 0) {
            printf("R%d: %d\n", i, state.registers[i]);
        }
    }
    printf("\n");

    // Final memory state
    printf("Final memory state:\n");
    for (int i = 0; i < 1024; i++) {
        if (memory_changed[i]) {
            printf("Address: %d, Contents: %u\n", i * 4, state.memory[i]);
        }
    }
    printf("\n");

    // Total clock cycles:
    // This clock_cycles is from the pipeline simulator (no_fwd.c)
    printf("Total stalls: %d\n", total_stalls);
    printf("Timing Simulator:\n");
    printf("Total number of clock cycles: %d\n", clock_cycles);
}

// Main function to run the simulator
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <memory_image_file> <FS|NF|WF>\n", argv[0]);
        return 1;
    }

    const char *memory_image_file = argv[1];
    const char *mode = argv[2];

    // Always initialize state before loading memory or running simulation
    initialize_machine_state();

    // Load memory image
    if (read_memory_image(memory_image_file, state.memory) < 0) {
        fprintf(stderr, "Error: Failed to load memory image from file '%s'\n", memory_image_file);
        return 1;
    }

    // IMPORTANT: For NF mode, the state.registers must be initialized *before*
    // the pipeline simulation starts, ideally by functionally executing the initial
    // setup instructions. Since the problem requires the pipeline simulator to be
    // standalone, and not call functional_sim's loop, we need to ensure functional_sim
    // correctly reflects the state of the registers when it finishes its loop
    // (for FS mode), or that the registers are correctly updated by instructions
    // in the WB stage of the pipeline simulator.

    if (strcmp(mode, "FS") == 0) {
        // Run functional simulation loop
        // ADDED FS Trace Start
        printf("[FS_FOCUS_TRACE_START]\n");
        while (1) {
            uint32_t pc_before_simulate = state.pc;

            if (state.pc >= 4096) { // Check for PC out of bounds
                fprintf(stderr, "[FS_FOCUS_TRACE] PC out of bounds: %u\n", state.pc);
                break;
            }
            uint32_t instr_word = state.memory[state.pc / 4];
            DecodedInstruction decoded = decode_instruction(instr_word);

            /*
            // <<< ADDED BLOCK START >>>
            // Print key architectural state *before* the instruction is simulated (optional, but can be useful)
            printf("[FS_TRACE] PRE  PC=0x%03X: %s (Op:0x%X Rd:%d Rs:%d Rt:%d Imm:%d) || R1=%d R8=%d R10=%d R11=%d\n",
                    pc_before_simulate,
                    opcode_to_string(decoded.opcode), // Ensure opcode_to_string is available
                    decoded.opcode, decoded.rd, decoded.rs, decoded.rt, decoded.immediate,
                    state.registers[1], state.registers[8], state.registers[10], state.registers[11]);
            // <<< ADDED BLOCK END >>> */

            simulate_instruction(decoded);

            // Key PC logging
            uint32_t key_pcs[] = {0, 4, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96};
            int num_key_pcs = sizeof(key_pcs) / sizeof(uint32_t);
            int log_this_instruction = 0;
            for (int i = 0; i < num_key_pcs; i++) {
                if (state.pc == key_pcs[i]) {
                    log_this_instruction = 1;
                    break;
                }
            }
            // Always log branch decisions & HALT
            if (decoded.opcode == BEQ || decoded.opcode == BZ || decoded.opcode == JR || decoded.opcode == HALT) {
                log_this_instruction = 1;
            }

            if (log_this_instruction) {
                printf("[FS_COMMIT] PC=0x%03X; Op=%-4s(0x%02X); Rd=%2d,Rs=%2d,Rt=%2d,Imm=%-6d || R1=%-4d,R2=%-4d,R3=%-4d,R4=%-4d,R5=%-3d,R6=%-3d,R8=%-4d,R10=%-2d,R11=%-2d,R12=%-2d || NextPC=0x%03X\n",
                       state.pc, 
                       opcode_to_string(decoded.opcode), decoded.opcode,
                       decoded.rd, decoded.rs, decoded.rt, decoded.immediate,
                       state.registers[1], state.registers[2], state.registers[3], state.registers[4],
                       state.registers[5], state.registers[6], state.registers[8], state.registers[10],
                       state.registers[11], state.registers[12], 
                       state.pc); // state.pc is the PC for the *next* instruction
                fflush(stdout);
            }

            /* <<< ADDED BLOCK START >>>
            // Print key architectural state *after* the instruction has been simulated
            // Adjust which registers (R1-R12 or others) you print based on what's most relevant
            printf("[FS_TRACE] PC_Exec=0x%03X; Op=%-4s(0x%02X); Rd=%2d,Rs=%2d,Rt=%2d,Imm=%-6d || R1=%-4d R2=%-4d R3=%-4d R4=%-4d R5=%-4d R6=%-4d R7=%-4d R8=%-4d R9=%-4d R10=%-3d R11=%-3d R12=%-3d || Next_Arch_PC=0x%03X\n",
                   pc_before_simulate, // The PC of the instruction that JUST ran
                   opcode_to_string(decoded.opcode), // Make sure opcode_to_string is available and declared
                   decoded.opcode,
                   decoded.rd, decoded.rs, decoded.rt, decoded.immediate,
                   state.registers[1], state.registers[2], state.registers[3], state.registers[4],
                   state.registers[5], state.registers[6], state.registers[7], state.registers[8],
                   state.registers[9], state.registers[10], state.registers[11], state.registers[12],
                   state.pc); // The new architectural PC for the *next* instruction
            
            fflush(stdout); // Ensure output is printed immediately
            // <<< ADDED BLOCK END >>> */

            if (decoded.opcode == HALT) {
                break; // simulate_instruction() for HALT will call exit(0) and print_final_state()
            }
        }
        // If HALT doesn't exit, ensure print_final_state is called
        // print_final_state(); // Called by HALT instruction in simulate_instruction
        printf("[FS_FOCUS_TRACE_END]\n");
        print_final_state();
        return 0;

    } else if (strcmp(mode, "NF") == 0) {
        // Run no-forwarding pipeline simulator
        simulate_pipeline_no_forwarding();
        // Final state will be printed by the pipeline simulator (no_fwd.c) itself when HALT hits WB.
        // It might also call print_final_state from functional_sim if the HALT instruction in WB calls simulate_instruction.
        // As per previous problem context, this is handled.
        return 0;

    } else if (strcmp(mode, "WF") == 0) {
        // Run pipeline simulator with forwarding.
        simulate_pipeline_with_forwarding();
        return 0;

    } else {
        fprintf(stderr, "Error: Invalid mode. Use 'FS' for Functional Simulator, 'NF' for No Forwarding Pipeline Simulator, or 'WF' for Forwarding Pipeline Simulator.\n");
        return 1;
    }
}