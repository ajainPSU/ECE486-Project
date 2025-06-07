// no_fwd.h (Modified)
#ifndef NO_FWD_H
#define NO_FWD_H

#include "instruction_decoder.h" // Include this for DecodedInstruction
#include <stdint.h>              // For uint32_t

// Pipeline stage names for readability
enum pipeline_stages { IF = 0, ID, EX, MEM, WB };

// Struct to hold pipeline state (UPDATED)
typedef struct {
    DecodedInstruction instr;
    int valid; // 1 if real instruction, 0 if NOP or flushed
    uint32_t pc; // Program counter of this instruction (for debugging/tracing within pipeline)
    int branch_taken; // Flag for taken branches (set in EX)
    uint32_t branch_target; // Target address for taken branches (set in EX)
    int32_t result_val; // Value to be written to register (from EX/MEM) or loaded value
} PipelineRegister;

// Global NOP_INSTRUCTION instance declaration (defined in global_counters.c)
extern DecodedInstruction NOP_INSTRUCTION;

// Function declarations common to pipeline simulation (can be used by both no_fwd.c and with_fwd.c)
int is_nop(DecodedInstruction instr); // Declare is_nop here
// Helper functions for pipeline management
void insert_nop(int stage, PipelineRegister pipeline_arr[]);
void initialize_pipeline(PipelineRegister pipeline_arr[]);

// Function declarations for no_fwd.c specific functions
void simulate_pipeline_no_forwarding();


#endif // NO_FWD_H