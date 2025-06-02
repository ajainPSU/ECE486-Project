#ifndef NO_FWD_H
#define NO_FWD_H

// Pipeline stage names for readability
enum pipeline_stages { IF = 0, ID, EX, MEM, WB };

// Function declarations
void simulate_pipeline_no_forwarding();
void insert_nop(int stage);
void initialize_pipeline();
void advance_pipeline();
int detect_raw_hazard(DecodedInstruction curr, DecodedInstruction ex, DecodedInstruction mem, DecodedInstruction wb);

#endif // NO_FWD_H

/* Previous Implementation
#ifndef NO_FWD_H
#define NO_FWD_H

#include "instruction_decoder.h"

// Extern declaration for global counters
extern int clock_cycles;

// Enum for pipeline stages
enum pipeline_stages { IF = 0, ID, EX, MEM, WB };

// Function prototypes for the no-forwarding pipeline simulator
void initialize_pipeline();
void simulate_pipeline_no_forwarding();

#endif // NO_FWD_H
*/