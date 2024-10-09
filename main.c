#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"
#include "lobj.h"
#include "memory.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif//may need to use editline for mac linux repl


typedef enum {
    SEXPR_NUMBER,
    SEXPR_SYMBOL,
    SEXPR_STRING,
    SEXPR_LIST
} SExprType;

    typedef struct SExpr SExpr;

    struct SExpr {
        SExprType type;
        union {
            double number;
            char* symbol;
            char* string;
        } value;
        SExpr* next;
    };

bool is_identifier_char(char c) {
    return isalnum(c) || c == '_' || c == '-' || c == '+' || c == '*' || c == '/' || c == '<' || c == '>' || c == '=' || c == '!';
}

void ignore_spaces(const char* input, int* pos) {
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

SExpr* read_number(const char* input, int sign, int* pos) {
    char c = '\0';
    int64_t result = 0;
    for (char c = input[*pos]; isdigit(c); c = next(input, pos)) {
        result *= 10;
        result += c - '0';
    }
    SExpr* expr = ALLOCATE(SExpr, 1);
    expr->type = SEXPR_NUMBER;
    expr->value.number = sign * result;
    return expr;
}

SExpr* read_symbol(const char* input, int* pos) {
    int capacity = 256; 
    char* symbol = calloc(capacity, sizeof(char));
    int i = 0;
    char c = input[*pos];
    // fixme: buffer overflow
    while (is_identifier_char(c)) {
        symbol[i++] = c;
        c = input[++(*pos)];
    }
    SExpr* expr = ALLOCATE(SExpr, 1);
    if (strcmp(symbol, "true") == 0) {
        expr->type =  SEXPR_NUMBER;
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

SExpr* read_string(const char* input, int* pos) {
    int capacity = 256; 
    char* str = calloc(capacity, sizeof(char));
    int i = 0;
    bool escape = false;
    (*pos)++; // skip '"'
    char c = input[*pos];
    while (c != '"' || escape) {
        if(c == '\0') {
            printf("unclosed string\n");
            exit(1);
        }
        if(i >= capacity) {
            capacity *= 2;
            str = realloc(str, capacity);
            if(str == NULL) {
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
    SExpr* expr = ALLOCATE(SExpr, 1);
    expr->type = SEXPR_STRING;
    expr->value.string = str;
    return expr;
}


SExpr* read_atom(const char* input, int* pos);
SExpr* read_list(const char* input, int* pos);

SExpr* read_atom(const char* input, int* pos) {
    char cur = skip_whitespace(input, pos);
    printf("cur %c\n", input[*pos]);
    if (isdigit(cur)) {
        return read_number(input, 1, pos);
    }

//    if (cur == '-') {
//        if(input[*pos + 1] == '\0') {
//            printf("Invalid number\n");
//        }
//        (*pos)++;
//        return read_number(input, -1, pos);
//    }
    if (is_identifier_char(cur)) {
        return read_symbol(input, pos);
    }
    if (cur == '"') {
        SExpr* expr = read_string(input, pos);
        printf("len of string %zu\n", strlen(expr->value.string));
        return expr;
    }

    fprintf(stderr, "Failed to read: '%c'\n", cur);
    exit(1);
}
// recursively reads a list.
// starts off with root node (cur)
SExpr* read_list(const char* input, int* pos) {
    advance(pos); // skip the first '('
    SExpr* head = ALLOCATE(SExpr, 1);
        head->type = SEXPR_LIST;
        head->next = NULL;
    SExpr* cur = NULL;
    while(*pos < strlen(input) && input[*pos] != ')') {
        char next = skip_whitespace(input, pos);
        SExpr* el;
        if(next == '(') {
            el = read_list(input, pos);
        }  else {
            el = read_atom(input, pos);
        }
        if(cur == NULL) {
             cur = el;
             head->next = cur;
        } else {
             cur->next = el;
             cur = cur->next;
        }
    }
    cur->next = NULL;
    skip_whitespace(input, pos);
    advance(pos);
    return head;
}

SExpr** read_all(const char* input, size_t* count) {
    SExpr** exprs = NULL;
    size_t capacity = 0;
    *count = 0;
    int i = 0;
    while (i < strlen(input)) {
        char cur = skip_whitespace(input, &i);
        //printf("cur: '%c'\n", cur);
        if (cur == ';') {
            while (input[i] != '\0' && input[i] != '\n') i++;
            continue;
        }

        SExpr* expr;
        if(cur == '(') {

            expr = read_list(input, &i);
        } else {
            expr = read_atom(input, &i);
        }
        
        if (*count == capacity) {
            capacity = capacity ? capacity * 2 : 1;
            exprs = realloc(exprs, capacity * sizeof(SExpr*));
        }
        exprs[(*count)++] = expr;
        i++;
    }
    
    return exprs;
}
void print_sexpr(SExpr* expr);

void free_sexpr(SExpr* expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case SEXPR_SYMBOL:
            free(expr->value.symbol);
            break;
        case SEXPR_LIST: {
            SExpr* cur = expr->next;
            while(cur != NULL) {
                // Store the next node before deleting current
                SExpr* temp = cur->next;
                //printf("is null %d\n", temp == NULL);
                // Delete the current node
                free_sexpr(cur);
                // Move to the next node
                cur = temp;
            }
        } break;
        case SEXPR_STRING:
            free(expr->value.string);
            break;
        case SEXPR_NUMBER: break; // no op
        default:
            printf("unknown thing to free.\n");
            break;
    }
    //we free the wrapped SExpression struct
    free(expr);
}
// Helper function to print SExpr (for testing)
void print_sexpr(SExpr* expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case SEXPR_NUMBER:
            printf("(num %f)", expr->value.number);
            break;
        case SEXPR_SYMBOL:
            printf("(sym `%s`)", expr->value.symbol);
            break;
        case SEXPR_STRING:
            printf("(str \"%s\")", expr->value.string);
            break;
        case SEXPR_LIST:
            printf("(li");
            SExpr* cur = expr->next;
            while(cur != NULL) {
                print_sexpr(cur); printf(" ");
                cur = cur->next;
            }
            printf(")");
            break;
        default:
            printf("UNKNOWN TYPE WHILE PRINTING");
            break;
    }
}
int main(int argc, char** argv) {
  puts("Version 0.0.0.0.3");
  puts("Press Ctrl+c to Exit\n");
 
    /* Define them with the following Language */
//  mpca_lang(MPCA_LANG_DEFAULT,
//    "                                                     \
//        number   : /-?[0-9]+\\.?[0-9]*/ ;                 \
//        symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;     \
//        sexpr    : '(' <expr>* ')' ;                      \
//        expr     : <number> | <symbol> | <sexpr>;         \
//        prog : /^/ <expr>* /$/ ;                          \
//    ",
//    Number, Symbol, Sexpr, Expr, Euclase);
//Create a repl environment
//  while(1) {
//    char* input = readline("euclase>");
//    if (input && *input)
//        add_history(input);
    size_t count;
    int i = 0;
    SExpr** exprs = read_all("(2 ( 1 2 ) 2)", &count);
    
    for (size_t i = 0; i < count; i++) {
        print_sexpr(exprs[i]);
        printf(" ");
    }
    printf("\n");
    //free(input);
    printf("Num expressions: %zu\n", count);
    for (size_t i = 0; i < count; i++) {
        free_sexpr(exprs[i]);
    }
//}
//  return 0;
}
