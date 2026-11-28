#include "io.h"
#include <stdio.h>
#include <stdlib.h>

char* read_file(const char *filename, size_t *size_read) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    // Get file size. Pipes and other non-seekable inputs make fseek/ftell
    // fail (ftell returns -1), which would size the buffer wrongly.
    long file_size = -1;
    if (fseek(file, 0, SEEK_END) == 0) file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "Error: Could not determine the size of '%s' (is it a pipe?)\n", filename);
        fclose(file);
        return NULL;
    }
    rewind(file);

    // Allocate buffer for entire file content
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    // Read file content
    *size_read = fread(buffer, 1, file_size, file);
    buffer[*size_read] = '\0';  // Null terminate the string
    fclose(file);

    return buffer;
}
