#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "asm.h"
#include "memory.h"
#include "reader.h"
#include "vm.h"


static void label_init(Label* l, char* name, int arity) { 
    l->code = NULL;
    l->arity = arity;
    l->capacity = 0;
    l->count = 0;
    l->constants = malloc(sizeof(lobj) * 256);  // Max 256 constants
    l->constants_count = 0;
    l->name = name;
    l->address=l->count;
}
static void labels_write(Assembler* assembler, char* name, int arity) {
    Label* l = ALLOCATE(Label, 1);
    label_init(l, name, arity);
    assembler->current_label = l;
    //printf("setting current label to %s\n", name);
    darray_push(assembler->labels, l);
}

static Label* labels_find(LabelArray* array, const char* name) {
    Label** labl;
    darray_foreach(labl, *array) {
        if (strcmp((*labl)->name, name) == 0) {
            return *labl;
        }
    }
    printf("found nothind\n");
    return NULL;
}

void asm_init(Assembler* a) {
    darray_init(a->labels);
}

static void write_byte(Assembler* a, uint8_t byte) {
    Label* current_label = a->current_label;
    if (current_label->capacity < current_label->count + 1) {
        int new_capacity = current_label->capacity == 0 ? 8 : current_label->capacity * 2;
        current_label->code = realloc(current_label->code, new_capacity);
        current_label->capacity = new_capacity;
    }
    
    current_label->code[current_label->count] = byte;
    current_label->count++;
    //printf("WRITING %d to %s\n", byte, current_label->name);
}

static void write_uint16_t(Assembler* a, uint16_t value) {
    write_byte(a, (value >> 8) & 0xFF);
    write_byte(a, value & 0xFF);
}

static int add_constant(Assembler* a, lobj value) {
    Label* cur = a->current_label;
    if (cur->constants_count >= 256) {
        printf("Too many constants in one chunk.\n");
        exit(1);
    }
    cur->constants[cur->constants_count] = value;
    return cur->constants_count++;
}

static long parse_number(const char* token) {
    char* end;
    long value = strtol(token, &end, 10);
    if (*end != '\0') {
        printf("Invalid number: %s\n", token);
        exit(1);
    }
    return value;
}
static char* read_token(const char* source, int* pos) {
    char cur = skip_whitespace(source, pos);
    //accumulate next token
    char* token = calloc(256, sizeof(char));
    int len = 0;
    while(len < 256 && !isspace(cur) && cur != '\0') {
        token[len] = cur;
        cur=source[++(*pos)];
        len++;
    }
    if(len == 0) {
        return NULL;
    }
    //printf("%s %llu\n", token, strlen(token));
    return token;
}

// https://github.com/rustyrussell/ccan/blob/161fe383533c12b95d9117a7ab75b5d3513287b0/ccan/json/json.c#L38C1-L45C2
static char *asm_strdup(const char *str)
{
	char *ret = (char*) malloc(strlen(str) + 1);
	if (ret == NULL){
            fprintf(stderr, "out of memory");
	    exit(1);
        }
	strcpy(ret, str);
	return ret;
}

int asm_compile(Assembler* a, const char* source) {
    int pos = 0;

    while (pos < strlen(source) && source[pos] != '\0') {
        while(isspace(source[pos])) { pos++; }
        if (source[pos] == ';') {
            while(source[pos] != '\n') pos++;
            continue;
        }
        
        char* token = read_token(source, &pos);
        if(token == NULL) {
            continue;
        }
        // Check if it's a label definition
        if (token[0] == ':') {
            //FIX ME: 
            if (strlen(token) == 1) {
                fprintf(stderr, "Label must have alphanumeric characters greater than length one\n");
                exit(1);
            }
            //todo, asm label might be length one, need to check
            char* tokname = asm_strdup(token + 1);
            char* arity = read_token(source, &pos);
            labels_write(a, tokname, parse_number(arity));
            free(token); free(arity);
            continue;
        }
#if DEBUG
        printf("token: %s, len: %llu\n", token, strlen(token));
#endif
        // Parse instruction
        if (strcmp(token, "HALT") == 0) {
            write_byte(a, I_HALT);
        } else if (strcmp(token, "LOAD") == 0) {
            write_byte(a, I_LOAD);
            char* operand = read_token(source, &pos);
            if(operand[0] == ':') {
                char* tokname = operand+1;
                Label* addr = labels_find(&a->labels, tokname);
                if(!addr) {
                    printf("Could not find label %s", tokname);
                    exit(1);
                }
                int constant = add_constant(
                    a, 
                    lobj_func(addr->code, addr->count, addr->constants, addr->constants_count, addr->arity, lobj_str(tokname)));
                write_byte(a, constant);
            } else {
                int value = parse_number(operand);
                int constant = add_constant(a, lobj_num(value));
                write_byte(a, constant);
            }
        } else if (strcmp(token, "ADD") == 0) {
            write_byte(a, I_ADD);
        } else if (strcmp(token, "SUB") == 0) {
            write_byte(a, I_SUB);
        } else if (strcmp(token, "MUL") == 0) {
            write_byte(a, I_MUL);
        } else if (strcmp(token, "DIV") == 0) {
            write_byte(a, I_DIV);
        } else if (strcmp(token, "MOD") == 0) {
            write_byte(a, I_MOD);
        } else if (strcmp(token, "NEG") == 0) {
            write_byte(a, I_NEGATE);
        } else if (strcmp(token, "POP") == 0) {
            write_byte(a, I_POP);
        } else if (strcmp(token, "RETURN") == 0) {
            write_byte(a, I_RETURN);
        } else if (strcmp(token, "CALL") == 0) {
            write_byte(a, I_CALL);
            char* operand = read_token(source, &pos);
            write_byte(a, parse_number(operand));
            free(operand);
        } else if (strcmp(token, "GET_LOCAL") == 0) {
            write_byte(a, I_GET_LOCAL);
            free(token);
            token = read_token(source, &pos);
            write_byte(a, parse_number(token));
        } else if (strcmp(token, "SET_LOCAL") == 0) {
            write_byte(a, I_SET_LOCAL);
            free(token);
            token = read_token(source, &pos);
            write_byte(a, parse_number(token));
        } else if (strcmp(token, "JMP") == 0) {
            write_byte(a, I_JMP);
            token = read_token(source, &pos);
            int offset = parse_number(token);
            write_uint16_t(a, offset);
        } else if (strcmp(token, "JMP_FALSY") == 0) {
            write_byte(a, I_JMP_FALSY);
            free(token);
            token = read_token(source, &pos);
            uint16_t offset = parse_number(token);
            write_uint16_t(a, offset);

        } else if (strcmp(token, "LOOP") == 0) {
            write_byte(a, I_LOOP);
            free(token);
            token = read_token(source, &pos);
            uint16_t offset = parse_number(token);
            write_uint16_t(a, offset);

        } else {
            printf("Unknown instruction: '%d'\n", token[0]);
            exit(1);
        }
        //Token is freed after creation
        free(token);
    }
    
    return 1;
}

lobj asm_get_entry(Assembler* a) {
    Label* l = labels_find(&a->labels, "main"); 
    if(l == NULL) {
        printf("Could not find entrypoint?\n");
        exit(0);
    }

    return lobj_func( 
        l->code, l->count,
        l->constants, l->constants_count,
        l->arity, lobj_str("__main__")
    );
}

void asm_free(Assembler* a) {
    // Free label names
    Label** labl;
    darray_foreach(labl, a->labels) {
        free((*labl)->name);
        free(*labl);
    }
    darray_free(a->labels);
}
const char* add =
    "; Simple program to add two numbers (3 + 4)\n"
    "; Result should be 7                       \n"
    ":add 0                                     \n"
    "    LOAD 1                                 \n"
    "    LOAD 2                                 \n"
    "    ADD                                    \n"
    "    RETURN                                 \n"

    ":main 0                                   \n"
    "   LOAD :add                              \n"
    "   CALL 0                                 \n"
    "   HALT            ; Stop program and show result\n";


const char* add_arity_2 = 
    "; Simple program to add two numbers (3 + 4)\n"
    "; Result should be 7                       \n"
    ":add 2                                     \n"
    "   ADD                                     \n"
    "   RETURN                                  \n"

    ":main 0                                   \n"
    "   LOAD :add                              \n"
    "   LOAD 3                                 \n"
    "   LOAD 4                                 \n"
    "   CALL 2                                 \n"
    "   RETURN                                 \n";

const char* ifelse = 
    ":main 0                                   \n"
    "   LOAD 1                                 \n"
    "   JMP_FALSY 3                            \n"
    "   LOAD 4                                 \n"
    "   RETURN                                 \n"
    "   LOAD 3                                 \n"
    "   RETURN                                 \n";
   
const char* infinite_loop =
    ":main 0                                   \n"
    "   LOAD 1                                 \n"
    "   JMP_FALSY 4                            \n"
    "   POP                                    \n"
    "   LOOP 9                                 \n"
    "   RETURN                                 \n";

const char* local_variables = 
    ":main 0                                   \n"
    "   LOAD 1                                 \n"
    "   LOAD 20                                \n"
    "   ADD                                    \n"
    "   SET_LOCAL 0                            \n"
    "   GET_LOCAL 0                            \n"
    "   RETURN                                 \n";
 

int main(int argc, char** args) {
    Assembler assembler;
    asm_init(&assembler);
    asm_compile(&assembler, local_variables);
    
    VM vm;
    HashTable globals, strings;
    table_create(&globals);
    table_create(&strings);
    lobj main = asm_get_entry(&assembler);
    vm_create(&vm, &globals, &strings, &main);
    int result = vm_exec(&vm, 0);
    if(result) {
        printf("%s", vm_stacktrace(&vm));
    }
    asm_free(&assembler);
    vm_free(&vm);
    
}


