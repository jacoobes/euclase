// reader.c
#include "reader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

bool is_identifier_char(const char c) {
    return isalnum(c) ||
        c == '_' || c == '?' ||
        c == '-' || c == '+' || c == '*' || c == '/' || c == '<' || c == '>' || c == '=' || c == '@' || c == '%';
}

void ignore_spaces(const char *input, int *pos) {
    while (isspace(input[*pos])) (*pos)++;
}

void advance(int *pos) { ++*pos; }
char peek(const char *input, int *pos) {
    return input[(*pos) + 1];
}

char next(const char *input, int *pos) {
    advance(pos);
    return input[*pos];
}

char skip_whitespace(const char *input, int *pos) {
    char c = '\0';
    for (c = input[*pos]; isspace(c); c = next(input, pos)) {
        ;
    }
    return c;
}

SExpr* sexpr_number_create(double num) {
    SExpr *expr = ALLOC_SEXPR();
    expr->type = SEXPR_NUMBER;
    expr->value.number = num;
    return expr;
}

SExpr *read_number(const char *input, const int sign, int *pos) {
    double result = 0;
    for (char c = input[*pos]; isdigit(c); c = next(input, pos)) {
        result *= 10;
        result += c - '0';
    }
    if(input[*pos] == '.') {
        advance(pos);
        double factor = 0.1;
        for (char c = input[*pos]; isdigit(c); c = next(input, pos)) {
            result += (c - '0') * factor;
            factor *= 0.1;
        }
    }
    return sexpr_number_create(sign*result);
}

SExpr *read_symbol(const char *input, int *pos) {
    // Fixed-size buffer: a symbol can hold at most capacity - 1 characters,
    // because the last byte is needed for the NUL terminator.
    const int capacity = 256;

    // 1. Measure the symbol first. *pos points at its first character and it
    //    ends at the first character that cannot be part of an identifier.
    int len = 0;
    while (is_identifier_char(input[*pos + len])) len++;

    // 2. Check it fits before writing anything into the buffer.
    if (len >= capacity) {
        fprintf(stderr, "Symbol too long: %d characters (max %d)\n", len, capacity - 1);
        exit(1);
    }

    // 3. Copy it. calloc zero-fills, so symbol[len] is already the NUL.
    char *symbol = calloc(capacity, sizeof(char));
    memcpy(symbol, input + *pos, len);
    *pos += len; // leave *pos on the character after the symbol

    SExpr *expr = ALLOC_SEXPR();
    if (strcmp(symbol, "true") == 0) {
        expr->type = SEXPR_NUMBER;
        expr->value.number = 1;
    } else if (strcmp(symbol, "false") == 0) {
        expr->type = SEXPR_NUMBER;
        expr->value.number = 0;
    } else {
        expr->type = SEXPR_SYMBOL;
        expr->value.symbol = symbol;
    }
    return expr;
}

SExpr *read_string(const char *input, int *pos) {
    int capacity = 256;
    char *str = calloc(capacity, sizeof(char));
    int i = 0;
    bool escape = false;
    (*pos)++; // skip '"'
    char c = input[*pos];
    while (c != '"' || escape) {
        if (c == '\0') {
            printf("unclosed string\n");
            exit(1);
        }
        // grow before the buffer is full: one byte must stay free for the NUL
        if (i + 1 >= capacity) {
            capacity *= 2;
            str = realloc(str, capacity);
            if (str == NULL) {
                printf("while reallocating buffer, it returned null");
                exit(1);
            }
        }
        if (escape) {
            switch (c) {
                case 'n': str[i++] = '\n'; break;
                case 't': str[i++] = '\t'; break;
                case 'r': str[i++] = '\r'; break;
                case '"': str[i++] = '\"'; break;
                default: str[i++] = c; break;
            }
            escape = false;
        } else {
            if (c == '\\') {
                escape = true;
            } else {
                str[i++] = c;
            }
        }
        c = input[++(*pos)];
    }
    // terminate explicitly: realloc does not zero the bytes it adds
    str[i] = '\0';
    (*pos)++; // skip '"'
    SExpr *expr = ALLOC_SEXPR();
    expr->type = SEXPR_STRING;
    // string is heap allocated
    expr->value.string = str;
    return expr;
}

SExpr *read_atom(const char *input, int *pos) {
    const char cur = skip_whitespace(input, pos);
    const char next = peek(input, pos);
    if (isdigit(cur)) {
        return read_number(input, 1, pos);
    }
    if (cur == '-' && isdigit(next)) {
        advance(pos);
        return read_number(input, -1, pos);
    }
    if (is_identifier_char(cur)) {
        return read_symbol(input, pos);
    }
    if (cur == '"') {
        return read_string(input, pos);
    }
    fprintf(stderr, "Failed to read: '%d' while parsing an atom\n", cur);
    exit(1);
}
SExpr*  sexpr_list_create() {
    SExpr* head = ALLOC_SEXPR();
    head->type = SEXPR_LIST;
    head->value.cons.first = NULL;
    head->value.cons.rest = NULL;
    return head;
}


SExpr* read_list(const char* input, int* pos) {
    advance(pos); // skip the first '('
    SExpr* head = NULL;
    SExpr* cur = NULL;
    int len = strlen(input);
    char next;
    while(*pos < len && (next = skip_whitespace(input, pos))) {
        //handle empty list
        if (next == ')') {
            break;
        }
        SExpr* el;
        if(next == '(') {
            el = read_list(input, pos);
        } else {
            el = read_atom(input, pos);
        }


        // Create new cons cell
        SExpr* new_cons = sexpr_list_create();
        //setting the value we found to the first one,
        new_cons->value.cons.first = el;

        // Handle first element
        if(head == NULL) {
            head = new_cons;
            cur = head;
        } else {
            cur->value.cons.rest = new_cons;
            cur = new_cons;
        }
    }

    //skip any trailing whitespace in list
    char ch = skip_whitespace(input, pos);
    if (ch != ')') {
        // hit end of input: advancing here would walk past the NUL
        fprintf(stderr, "Unterminated list: expected ')'\n");
        exit(1);
    }
    // skip the ')'
    advance(pos);
    if(!cur) {
        return sexpr_list_create();
    }
    return head;
}

SExpr **read_all(const char *input, size_t *count) {
    SExpr **exprs = NULL;
    size_t capacity = 0;
    *count = 0;
    int i = 0;
    while (i < strlen(input)) {
        char cur = skip_whitespace(input, &i);
        // only whitespace was left: stop here, read_atom would peek past the NUL
        if (cur == '\0') break;
        //printf("%c %d\n", cur, i);
        if (cur == ';') {
            while (input[i] != '\0' && input[i] != '\n') i++;
            continue;
        }
        SExpr *expr;
        if (cur == '(') {
            expr = read_list(input, &i);
        } else {
            expr = read_atom(input, &i);
        }
        if (*count == capacity) {
            capacity = capacity ? capacity * 2 : 1;
            exprs = realloc(exprs, capacity * sizeof(SExpr *));
        }
        if(!exprs) {
            fprintf(stderr, "Failed to allocate memory for expressions\n");
            exit(1);
        }
        exprs[(*count)++] = expr;
    }
    return exprs;
}

void print_atom(SExpr *cur) {
    switch (cur->type) {
        case SEXPR_NUMBER:
            printf("%f ", cur->value.number);
        break;
        case SEXPR_SYMBOL:
            printf("`%s `", cur->value.symbol);
        break;
        case SEXPR_STRING:
            printf("\"%s\" ", cur->value.string);
        break;
        case SEXPR_LIST:
            printf("(cons");
                print_sexpr(cur->value.cons.first); print_sexpr(cur->value.cons.rest);
            printf(")");
        break;
    }
}

void print_sexpr(SExpr *expr) {
    if (expr == NULL) return;
    print_atom(expr);
}

void free_sexpr(SExpr *expr) {
    switch (expr->type) {
        case SEXPR_SYMBOL:
            free(expr->value.symbol);
            break;
        case SEXPR_STRING:
            free(expr->value.string);
            break;
        case SEXPR_LIST: {
            SExpr* head = sexpr_list_first(expr);
            if(head != NULL) {
                free_sexpr(head);
            }
            SExpr* rest = sexpr_list_rest(expr);
            if(rest != NULL) {
                free_sexpr(rest);
            }
        } break;
        case SEXPR_NUMBER: {} break;
    }
}
