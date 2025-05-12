/*
* Alex Jain - 05/11/2025
* ECE 486 / Memory Trace Reader
* This program reads a memory image file and prints the address and data in a formatted manner.
* The memory image file contains lines of hexadecimal data, each representing a word in memory.
* The program assumes a word size of 4 bytes and a maximum memory size of 4KB (1024 lines).
*
* Version 1.0 - Initial Version
* To do: Add additional error handling, and convert each line to binary to pass to instruction decoder.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_SIZE 4 // Each word is 4 bytes
#define MAX_LINE_LENGTH 16 // Maximum length of a line in the file
#define MAX_MEMORY_LINES 1024 // 4KB memory limit (1024 lines)

void read_memory_image(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    unsigned int address = 0; // Start address at 0
    unsigned int line_count = 0;

    printf("Address\t\tData\n");
    printf("-------------------------\n");

    while (fgets(line, sizeof(line), file)) {
        if (line_count >= MAX_MEMORY_LINES) {
            fprintf(stderr, "Memory limit exceeded: %d lines\n", MAX_MEMORY_LINES);
            fclose(file);
            exit(EXIT_FAILURE);
        }
        // Remove newline character if present
        line[strcspn(line, "\n")] = '\0';

        // Print the address and the data
        printf("0x%08X\t%s\n", address, line);

        // Increment the address by 4 (word size)
        address += WORD_SIZE;
        line_count++;
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <memory_image_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    read_memory_image(argv[1]);

    return EXIT_SUCCESS;
}