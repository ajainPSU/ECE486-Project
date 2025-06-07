// with_fwd.h (Modified)
#ifndef WITH_FWD_H
#define WITH_FWD_H

#include <stdint.h>
#include "instruction_decoder.h"
#include "functional_sim.h" // For MachineState state; and extern global counters (though WF uses its own for specific output)
#include "no_fwd.h"         // To get PipelineRegister and enum pipeline_stages, NOP_INSTRUCTION

// Function prototype for the main pipeline simulation with forwarding
void simulate_pipeline_with_forwarding();

// Extern declarations for global counters (defined in global_counters.c)
extern int clock_cycles;
extern int total_stalls;
extern int total_flushes; // Not printed for WF, but tracked internally

// Helper function prototypes used by with_fwd.c (if not already in common headers)
int get_dest_reg(DecodedInstruction instr); // Assuming this is now globally accessible
int is_source_reg(DecodedInstruction instr, int reg_num); // Assuming this is now globally accessible
int is_nop(DecodedInstruction instr); // From no_fwd.h
void insert_nop(int stage, PipelineRegister pipeline_arr[]); // From no_fwd.h
void initialize_pipeline(PipelineRegister pipeline_arr[]); // From no_fwd.h
// In with_fwd.h
int instr_writes_to_reg(DecodedInstruction instr);

#endif // WITH_FWD_H