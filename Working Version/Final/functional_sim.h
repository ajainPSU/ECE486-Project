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

// Instruction type externs
extern int total_instructions;
extern int arithmetic_instructions;
extern int logical_instructions;
extern int memory_access_instructions;
extern int control_transfer_instructions;

// Declare clock_cycles as extern (defined in no_fwd.c)
extern int clock_cycles;
extern int total_stalls;

// Register Written Array (for final output tracking)
extern int register_written[32];
extern int memory_changed[1024];

// Extern declaration so other modules can access it
extern MachineState state;

// Function prototypes
void initialize_machine_state();
void simulate_instruction(DecodedInstruction instr);
void print_final_state();

#endif // FUNCTIONAL_SIMULATOR_H