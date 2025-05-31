/*
* Alex Jain - 05/30/2025
* ECE 486 / Functional Simulator
* 
* Takes elements from instruction_decoder.c and trace_reader.c
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

/*
// Machine state
typedef struct {
    uint32_t pc;           // Program Counter
    int32_t registers[32]; // General-purpose registers (R1 to R31)
    uint32_t memory[1024]; // Simulated memory (4KB)
} MachineState; */

// Register Written Tracking
int register_written[32] = {0};

MachineState state; // Global machine state

// Instruction counters
int total_instructions = 0;
int arithmetic_instructions = 0;
int logical_instructions = 0;
int memory_access_instructions = 0;
int control_transfer_instructions = 0;

// Memory change tracking
int memory_changed[1024] = {0}; // Tracks which memory addresses were modified

// Initialize the machine state
void initialize_machine_state() {
    state.pc = 0; // Initialize PC to 0
    memset(state.registers, 0, sizeof(state.registers)); // Initialize registers to 0
    memset(state.memory, 0, sizeof(state.memory)); // Initialize memory to 0
    memset(memory_changed, 0, sizeof(memory_changed)); // Reset memory change tracking
}

void simulate_instruction(DecodedInstruction instr) {
    total_instructions++;

    switch (instr.opcode) {
        // Arithmetic Instructions
        case ADD:
            state.registers[instr.rd] = state.registers[instr.rs] + state.registers[instr.rt];
            register_written[instr.rd] = 1; // Mark register as written
            arithmetic_instructions++;
            break;
        case ADDI:
            state.registers[instr.rt] = state.registers[instr.rs] + instr.immediate;
            register_written[instr.rt] = 1; // Mark register as written
            arithmetic_instructions++;
            break;
        case SUB:
            state.registers[instr.rd] = state.registers[instr.rs] - state.registers[instr.rt];
            register_written[instr.rd] = 1; // Mark register as written
            arithmetic_instructions++;
            break;
        case SUBI:
            state.registers[instr.rt] = state.registers[instr.rs] - instr.immediate;
            register_written[instr.rt] = 1; // Mark register as written
            arithmetic_instructions++;
            break;
        case MUL:
            state.registers[instr.rd] = state.registers[instr.rs] * state.registers[instr.rt];
            register_written[instr.rd] = 1; // Mark register as written
            arithmetic_instructions++;
            break;
        case MULI:
            state.registers[instr.rt] = state.registers[instr.rs] * instr.immediate;
            register_written[instr.rt] = 1; // Mark register as written
            arithmetic_instructions++;
            break;

        // Logical Instructions
        case OR:
            state.registers[instr.rd] = state.registers[instr.rs] | state.registers[instr.rt];
            register_written[instr.rd] = 1; // Mark register as written
            logical_instructions++;
            break;
        case ORI:
            state.registers[instr.rt] = state.registers[instr.rs] | instr.immediate;
            register_written[instr.rt] = 1; // Mark register as written
            logical_instructions++;
            break;
        case AND:
            state.registers[instr.rd] = state.registers[instr.rs] & state.registers[instr.rt];
            register_written[instr.rd] = 1; // Mark register as written
            logical_instructions++;
            break;
        case ANDI:
            state.registers[instr.rt] = state.registers[instr.rs] & instr.immediate;
            register_written[instr.rt] = 1; // Mark register as written
            logical_instructions++;
            break;
        case XOR:
            state.registers[instr.rd] = state.registers[instr.rs] ^ state.registers[instr.rt];
            register_written[instr.rd] = 1; // Mark register as written
            logical_instructions++;
            break;
        case XORI:
            state.registers[instr.rt] = state.registers[instr.rs] ^ instr.immediate;
            register_written[instr.rt] = 1; // Mark register as written
            logical_instructions++;
            break;

        // Memory Access Instructions
        case LDW: {
            int32_t address = state.registers[instr.rs] + instr.immediate;
            state.registers[instr.rt] = state.memory[address / 4];
            register_written[instr.rt] = 1; // Mark register as written
            memory_access_instructions++;
            break;
        }
        case STW: {
            int32_t address = state.registers[instr.rs] + instr.immediate;
            state.memory[address / 4] = state.registers[instr.rt];
            memory_changed[address / 4] = 1; // Mark memory as changed
            memory_access_instructions++;
            break;
        }

        // Control Transfer Instructions
        case BZ:
            if (state.registers[instr.rs] == 0) {
                state.pc += ((int32_t)instr.immediate) * 4;
                control_transfer_instructions++;
                return;
            }
            control_transfer_instructions++;
            break;
        case BEQ:
            if (state.registers[instr.rs] == state.registers[instr.rt]) {
                state.pc += ((int32_t)instr.immediate) * 4;
                control_transfer_instructions++;
                return;
            }
            control_transfer_instructions++;
            break;
        case JR:
            state.pc = state.registers[instr.rs];
            control_transfer_instructions++;
            return;
        case HALT:
            control_transfer_instructions++;
            state.pc += 4;
            printf("Program halted.\n");
            print_final_state();
            exit(0);

        default:
            printf("Unknown opcode: 0x%02X at PC: 0x%08X\n", instr.opcode, state.pc);
            return;
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
        if (register_written[i]) {
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
}

int func_sim_main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory_image_file>\n", argv[0]);
        return 1;
    }

    initialize_machine_state();

    // Step 1: Load the memory contents (just store values, don't execute)
    read_memory_image(argv[1], NULL);

    // Step 2: Begin simulation loop from PC = 0
    while (1) {
        if (state.pc >= 4096) {  // 4KB memory = 1024 words = 4096 bytes
            fprintf(stderr, "PC out of bounds: %u\n", state.pc);
            break;
        }

        uint32_t instr_word = state.memory[state.pc / 4];
        DecodedInstruction decoded = decode_instruction(instr_word);
        simulate_instruction(decoded);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    return func_sim_main(argc, argv);
}
