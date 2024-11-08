// reader.c
#include "reader.h"
#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

bool is_identifier_char(const char c) {
    return isalnum(c) || c == '_' || c == '-' || c == '+' || c == '*' || c == '/' || c == '<' || c == '>' || c == '=';
}

void ignore_spaces(const char *input, int *pos) {
    while (isspace(input[*pos])) (*pos)++;
}

void advance(int *pos) { ++*pos; }

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

SExpr *read_number(const char *input, int sign, int *pos) {
    int64_t result = 0;
    for (char c = input[*pos]; isdigit(c); c = next(input, pos)) {
        result *= 10;
        result += c - '0';
    }
    SExpr *expr = ALLOCATE(SExpr, 1);
    expr->type = SEXPR_NUMBER;
    expr->value.number = sign * result;
    expr->next = NULL;
    return expr;
}

SExpr *read_symbol(const char *input, int *pos) {
    int capacity = 256;
    char *symbol = calloc(capacity, sizeof(char));
    int i = 0;
    char c = input[*pos];
    while (is_identifier_char(c)) {
        symbol[i++] = c;
        c = input[++(*pos)];
    }
    SExpr *expr = ALLOCATE(SExpr, 1);
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
    expr->next = NULL;
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
        if (i >= capacity) {
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
    (*pos)++; // skip '"'
    SExpr *expr = ALLOCATE(SExpr, 1);
    expr->type = SEXPR_STRING;
    expr->value.string = str;
    expr->next = NULL;
    return expr;
}

SExpr *read_atom(const char *input, int *pos) {
    char cur = input[*pos];
    if (isdigit(cur)) {
        return read_number(input, 1, pos);
    }
    if (is_identifier_char(cur)) {
        return read_symbol(input, pos);
    }
    if (cur == '"') {
        return read_string(input, pos);
    }
    fprintf(stderr, "Failed to read: '%c'\n", cur);
    exit(1);
}

SExpr* read_list(const char* input, int* pos) {
    advance(pos); // skip the first '('
    SExpr* head = ALLOCATE(SExpr, 1);
    head->next = NULL;
    SExpr* cur = NULL;
    while(*pos < strlen(input) && input[*pos] != ')') {
        char next = skip_whitespace(input, pos);
        if(next == ')') {
            break;
        }
        SExpr* el;
        if(next == '(') {
            el = ALLOCATE(SExpr, 1);
            el->type = SEXPR_LIST;
            el->value.list = read_list(input, pos);
            el->next = NULL;
        } else {
            el = read_atom(input, pos);
        }
        if(cur == NULL) {
            cur = el;
            head = cur;
        } else {
            cur->next = el;
            cur = cur->next;
        }
    }
    if(!cur) {
        return head;
    }
    skip_whitespace(input, pos);
    advance(pos);
    return head;
}

SExpr **read_all(const char *input, size_t *count) {
    SExpr **exprs = NULL;
    size_t capacity = 0;
    *count = 0;
    int i = 0;
    while (i < strlen(input)) {
        char cur = skip_whitespace(input, &i);
        if (cur == ';') {
            while (input[i] != '\0' && input[i] != '\n') i++;
            continue;
        }
        SExpr *expr;
        if (cur == '(') {
            expr = ALLOCATE(SExpr, 1);
            expr->type = SEXPR_LIST;
            expr->value.list = read_list(input, &i);
            expr->next = NULL;
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
        i++;
    }
    return exprs;
}

void print_atom(SExpr *cur) {
    switch (cur->type) {
        case SEXPR_NUMBER:
            printf("(num %f)", cur->value.number);
        break;
        case SEXPR_SYMBOL:
            printf("(sym `%s`)", cur->value.symbol);
        break;
        case SEXPR_STRING:
            printf("(str \"%s\")", cur->value.string);
        break;
        case SEXPR_LIST:
            printf("(li ");
            print_sexpr(cur->value.list);
            printf(" )");
        break;
    }
}

void print_sexpr(SExpr *expr) {
    if (expr == NULL) return;
    SExpr *cur = expr;
    while (cur != NULL) {
        print_atom(cur);
        cur = cur->next;
    }
}

void free_sexpr(SExpr *expr) {
    while(NULL != expr) {
        switch (expr->type) {
            case SEXPR_SYMBOL:
                free(expr->value.symbol);
            break;
            case SEXPR_STRING:
                free(expr->value.string);
            break;
            case SEXPR_LIST:
                free_sexpr(expr->value.list);
            break;
            case SEXPR_NUMBER:
            break;
        }
        SExpr *next = expr->next;
        free(expr);
        expr = next;
    }
}