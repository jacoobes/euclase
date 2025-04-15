#ifndef COMPAT_H
#define COMPAT_H

// POSIX feature macros for strdup, getline, etc.
// Must be defined before any system includes
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <string.h>
#include <stdlib.h>

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32

static char buffer[2048];

/* Fake readline function */
char *readline(const char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the edit line headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif//_WIN32

#endif // COMPAT_H
