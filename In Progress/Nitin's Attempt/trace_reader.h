#ifndef TRACE_READER_H
#define TRACE_READER_H

#include <stdio.h>
#include <stdint.h> // Needed for uint32_t

// Constants
#define WORD_SIZE 4 // Each word is 4 bytes
#define MAX_LINE_LENGTH 16 // Maximum length of a line in the file
#define MAX_MEMORY_LINES 1024 // 4KB memory limit (1024 lines)

// Function prototypes
void print_binary(unsigned int value);
int read_memory_image(const char *filename, uint32_t *memory);

#endif // TRACE_READER_H
