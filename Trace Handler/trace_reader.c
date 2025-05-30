/*
* Alex Jain - 05/24/2025
* ECE 486 / Memory Trace Reader
* This program reads a memory image file and prints the address, data, and binary representation.
* The memory image file contains lines of hexadecimal data, each representing a word in memory.
* The program assumes a word size of 4 bytes and a maximum memory size of 4KB (1024 lines).
*
* Version 1.2 - Added toggleable debugging to test functionality.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trace_reader.h"

int DEBUG = 0; // Set to 1 to enable debug output, 0 to disable.

#define WORD_SIZE 4 // Each word is 4 bytes
#define MAX_LINE_LENGTH 16 // Maximum length of a line in the file
#define MAX_MEMORY_LINES 1024 // 4KB memory limit (1024 lines)

// Function to print hexadecimal value in binary format.
void print_binary(unsigned int value) {
    for (int i = 31; i >= 0; i--) {
        printf("%c", (value & (1U << i)) ? '1' : '0');
    }
}

// Function to read memory image file.
void read_memory_image(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    unsigned int address = 0; // Start address at 0
    unsigned int line_count = 0;

    if (DEBUG) {
        printf("Memory Image Reader Output:\n");
        printf("---------------------------------------------------------------\n");
        printf("Address\t\tData\t\tBinary\n");
        printf("---------------------------------------------------------------\n");
    }

    // Read each line from the file
    while (fgets(line, sizeof(line), file)) {
        if (line_count >= MAX_MEMORY_LINES) {
            fprintf(stderr, "Memory limit exceeded: %d lines\n", MAX_MEMORY_LINES);
            fclose(file);
            exit(EXIT_FAILURE);
        }
        // Remove newline character if present
        line[strcspn(line, "\n")] = '\0';

        // Convert hex string to unsigned int
        unsigned int value = (unsigned int)strtoul(line, NULL, 16);

        // Print the address, hex data, and binary representation
        if (DEBUG) {
            printf("0x%08X\t%s\t", address, line);
            print_binary(value);
            printf("\n");
        }

        // Pass the binary value to the Instruction Decoder (not implemented here)
        if (process_binary != NULL) {
            process_binary(value);
        } 

        // Increment the address by 4 (word size)
        address += WORD_SIZE;
        line_count++;
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <memory_image_file> [debug]\n", argv[0]);
        return EXIT_FAILURE;
    }
    /* 
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory_image_file>\n", argv[0]);
        return EXIT_FAILURE;
    } */

    // Check for debug parameter
    if (argc == 3 && strcmp(argv[2], "debug") == 0) {
        DEBUG = 1; // Enable debug output.
    }

    read_memory_image(argv[1]);

    return EXIT_SUCCESS;
}
