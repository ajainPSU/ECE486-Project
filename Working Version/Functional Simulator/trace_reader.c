/*
* Alex Jain - 05/24/2025
* ECE 486 / Memory Trace Reader
* This program reads a memory image file and prints the address, data, and binary representation.
* The memory image file contains lines of hexadecimal data, each representing a word in memory.
* The program assumes a word size of 4 bytes and a maximum memory size of 4KB (1024 lines).
*
* Version 1.1 - Added binary output for each memory word.
*/

#include "functional_sim.h"
#include "trace_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_SIZE 4             // Each word is 4 bytes
#define MAX_LINE_LENGTH 16      // Maximum length of a line in the file
#define MAX_MEMORY_LINES 1024   // 4KB memory limit (1024 lines)

void read_memory_image(const char *filename, void (*process_binary)(unsigned int)) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    unsigned int address = 0;       // Start address at 0.
    unsigned int line_count = 0;

    extern MachineState state;  // Bring in global memory state

    while (fgets(line, sizeof(line), file)) {
        if (line_count >= MAX_MEMORY_LINES) {
            fprintf(stderr, "Memory limit exceeded: %d lines\n", MAX_MEMORY_LINES);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        line[strcspn(line, "\n")] = '\0';  // Remove newline
        unsigned int value = (unsigned int)strtoul(line, NULL, 16);

        if (process_binary != NULL) {
            process_binary(value);
        } else {
            state.memory[address / 4] = value;
        }

        address += WORD_SIZE;
        line_count++;
    }

    fclose(file);
}

