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

int read_memory_image(const char *filename, uint32_t *memory) {
    printf("Attempting to open file: %s\n", filename); // Debugging output

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1; // Return -1 on failure
    }

    int word_count = 0;
    uint32_t value;
    while (fscanf(file, "%x", &value) == 1) {
        if (word_count >= 1024) { // Ensure we don't exceed memory bounds
            fprintf(stderr, "Error: Memory image exceeds 4KB limit\n");
            fclose(file);
            return -1; // Return -1 if memory limit is exceeded
        }
        memory[word_count++] = value;
    }

    fclose(file);
    return word_count; // Return the number of words read
}

/* Original Function
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
} */

