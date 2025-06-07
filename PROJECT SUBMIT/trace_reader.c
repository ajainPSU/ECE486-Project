/*
*
* ECE 486 / Memory Trace Reader
* This program reads a memory image file and prints the address, data, and binary representation.
* The memory image file contains lines of hexadecimal data, each representing a word in memory.
* The program assumes a word size of 4 bytes and a maximum memory size of 4KB (1024 lines).
*
* Supported Operations:
* - Read a memory image file
* - Print the contents of memory in a formatted way
* 
* Functions:
* - read_memory_image: Reads a memory image from a file and stores it in an array.
*/

#include "functional_sim.h"
#include "trace_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
* Function: read_memory_image
* ----------------------------
* Reads a memory image from a file and stores it in an array.
*/
int read_memory_image(const char *filename, uint32_t *memory) {
    DBG_PRINTF("Attempting to open file: %s\n", filename); // Debugging output

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1; // Return -1 on failure
    }

    int word_count = 0;
    uint32_t value;
    while (fscanf(file, "%x", &value) == 1) {
        if (word_count >= 1024) { // Ensure we don't exceed memory bounds
            DBG_PRINTF(stderr, "Error: Memory image exceeds 4KB limit\n");
            fclose(file);
            return -1; // Return -1 if memory limit is exceeded
        }
        memory[word_count++] = value;
    }

    fclose(file);
    return word_count; // Return the number of words read
}
