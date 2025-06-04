// global_counters.c
#include "instruction_decoder.h" // For DecodedInstruction, NOP, I_TYPE

int clock_cycles = 0;
int total_stalls = 0;
int total_flushes = 0;

DecodedInstruction NOP_INSTRUCTION = {
    .opcode = NOP, .type = I_TYPE, .rs = 0, .rt = 0, .rd = 0, .immediate = 0
};
