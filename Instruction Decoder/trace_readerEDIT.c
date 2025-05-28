#include "trace_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_SIZE 4
#define MAX_LINE_LENGTH 16
#define MAX_MEMORY_LINES 1024

void print_binary(unsigned int value) {
    for (int i = 31; i >= 0; i--) {
        printf("%c", (value & (1U << i)) ? '1' : '0');
    }
}

void read_memory_image(const char *filename, void (*process_binary)(unsigned int)) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    unsigned int address = 0;
    unsigned int line_count = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line_count >= MAX_MEMORY_LINES) {
            fprintf(stderr, "Memory limit exceeded: %d lines\n", MAX_MEMORY_LINES);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        line[strcspn(line, "\n")] = '\0';
        unsigned int value = (unsigned int)strtoul(line, NULL, 16);

        if (process_binary != NULL) {
            process_binary(value);
        }

        address += WORD_SIZE;
        line_count++;
    }

    fclose(file);
}
