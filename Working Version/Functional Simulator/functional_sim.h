#ifndef FUNCTIONAL_SIMULATOR_H
#define FUNCTIONAL_SIMULATOR_H

#include <stdint.h>
#include "instruction_decoder.h"

// Machine state structure
typedef struct {
    uint32_t pc;           // Program Counter
    int32_t registers[32]; // General-purpose registers (R1 to R31)
    uint32_t memory[1024]; // Simulated memory (4KB)
} MachineState;

// Register Written Array
extern int register_written[32];

// Extern declaration so other modules can access it
extern MachineState state;

// Function prototypes
void initialize_machine_state();
void simulate_instruction(DecodedInstruction instr);
void print_final_state();

#endif // FUNCTIONAL_SIMULATOR_H
