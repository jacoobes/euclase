#ifndef IO_H
#define IO_H

#include <stddef.h>

// Reads a whole file into a malloc'd, NUL-terminated buffer. Caller frees.
// Returns NULL (after printing why) if the file can't be read.
char* read_file(const char *filename, size_t *size_read);

#endif // IO_H
