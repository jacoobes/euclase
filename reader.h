// reader.h

#ifndef READER_H
#define READER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    SEXPR_NUMBER,
    SEXPR_SYMBOL,
    SEXPR_STRING,
    SEXPR_LIST,
} SExprType;

typedef struct SExpr SExpr;

struct SExpr {
    SExprType type;
    union {
        double number;
        char *symbol;
        char *string;
        struct {
            SExpr* first;
            SExpr* rest;
        } cons;
    } value;
};

static inline SExpr* sexpr_list_first (SExpr* e) {
    return e->value.cons.first;
}
static inline SExpr* sexpr_list_rest(SExpr* e) {
    return e->value.cons.rest;
}

#define sexpr_list_foreach(el, cons) \
    for ((el)=cons; ((el) != NULL); (el)=sexpr_list_rest((el)))


/**
 * Checks whether a given character can be part of an identifier.
 *
 * An identifier character can be an alphanumeric character, underscore '_',
 * dash '-', plus '+', asterisk '*', forward slash '/', less than '<', greater
 * than '>', or equal sign '='.
 *
 * @param c The character to check.
 * @return true if the character can be part of an identifier, otherwise false.
 */
bool is_identifier_char(const char c);

/**
 * Skips over any spaces in the input string starting from the current position.
 *
 * @param input The input string to be processed.
 * @param pos A pointer to the current position in the input string. The position
 *            will be updated to point to the first non-space character.
 */
void ignore_spaces(const char *input, int *pos);

/**
 * Advances the position in the input string by one character.
 *
 * @param pos A pointer to the position index to be advanced.
 */
void advance(int *pos);

/**
 * Advances the position pointer and returns the next character from the input string.
 *
 * @param input The input string from which characters are being read.
 * @param pos A pointer to the position in the input string. This position will be incremented.
 * @return The next character in the input string after the position is incremented.
 */
char next(const char *input, int *pos);

/**
 * Skips over any whitespace characters in the input string, updating the position.
 *
 * @param input The input string to scan for whitespace.
 * @param pos A pointer to the current position in the input string; this will be updated.
 * @return The first non-whitespace character found at or after the current position.
 */
char skip_whitespace(const char *input, int *pos);

/**
 * Reads a numeric value from the input string starting at the specified position.
 *
 * @param input The input string from which to read the number.
 * @param sign The sign of the number being read, where 1 indicates a positive number
 * and -1 indicates a negative number.
 * @param pos A pointer to the current position in the input string. The position
 * will be updated as characters are read.
 * @return A pointer to an SExpr structure representing the numeric value read from
 * the input string.
 */
SExpr *read_number(const char *input, int sign, int *pos);

/**
 * Reads a symbol from the input string starting at the given position.
 * A symbol is a sequence of identifier characters (letters, digits, or
 * specific symbols such as '_', '-', '+', '*', '/', '<', '>', '=').
 * Special symbol values "true" and "false" are converted to number
 * expressions with values 1 and 0, respectively.
 *
 * @param input Pointer to the input string.
 * @param pos Pointer to the current position in the input string.
 * @return Pointer to the SExpr representing the read symbol or special symbol value.
 */
SExpr *read_symbol(const char *input, int *pos);

/**
 * Reads a string from the input starting at the given position.
 * This function handles escape characters within the string.
 *
 * @param input The input string from which to read.
 * @param pos Pointer to the position index within the input string.
 *            This position will be updated as characters are read.
 * @return A pointer to an SExpr structure representing the parsed string.
 */
SExpr *read_string(const char *input, int *pos);

/**
 * @brief Reads an atomic S-expression (e.g., number, symbol, or string) from the input string.
 *
 * The function checks the current character at the position indicated by `pos`
 * and determines the type of atomic S-expression (number, symbol, or string).
 * It then delegates the reading to the appropriate function based on this type.
 *
 * @param input The input string containing the S-expression.
 * @param pos Pointer to the current position within the input string; it is updated as the S-expression is read.
 * @return SExpr* Pointer to the S-expression object read from the input.
 */
SExpr *read_atom(const char *input, int *pos);

/**
 * Reads a list expression from the given input string starting at the specified position.
 *
 * @param input The input string containing the list expression to be read.
 * @param pos A pointer to the current position in the input string. The position will be updated as the input is read.
 *
 * @return A pointer to the SExpr structure representing the list expression.
 */
SExpr *read_list(const char *input, int *pos);

/**
 * Reads all s-expressions from the given input string.
 *
 * @param input The input string containing s-expressions.
 * @param count Pointer to a size_t variable where the number of s-expressions read will be stored.
 * @return A dynamically allocated array of pointers to SExpr structures representing the parsed s-expressions. The caller is responsible for freeing this array and the SExpr structures.
 */
SExpr **read_all(const char *input, size_t *count);

/**
 * @brief Prints the given s-expression.
 *
 * This function takes a pointer to an s-expression and prints its contents
 * to standard output. The s-expression can be a number, symbol, string,
 * or a list of s-expressions.
 *
 * @param expr Pointer to the s-expression to print.
 */
void print_sexpr(SExpr *expr);

/**
 * Frees memory allocated for a given S-expression and its components.
 * This function will recursively free any nested S-expressions.
 *
 * @param expr A pointer to the S-expression to be freed. If the pointer is
 *             NULL, the function does nothing.
 */
void free_sexpr(SExpr *expr);

#endif // READER_H
