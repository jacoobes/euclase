#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "asm.h"
#include "memory.h"
#include "reader.h"
#include "vm.h"


static void label_init(Label* l, char* name) { 
    l->code = NULL;
    l->capacity = 0;
    l->count = 0;
    l->constants = malloc(sizeof(lobj) * 256);  // Max 256 constants
    l->constants_count = 0;
    l->name = name;
    l->address=l->count;
}
static void labels_write(Assembler* assembler, const char* name ) {
    Label l;
    label_init(&l, strdup(name));
    darray_push(assembler->labels, l);
}

static Label* labels_find(LabelArray* array, const char* name) {
    Label* labl;
    darray_foreach(labl, *array) {
        if (strcmp(labl->name, name) == 0) {
            return labl;
        }
    }
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
}

static void write_short(Assembler* a, uint16_t value) {
    write_byte(a, (value >> 8) & 0xff);
    write_byte(a, value & 0xff);
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

static int parse_number(const char* token) {
    char* end;
    int value = strtol(token, &end, 10);
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
    while(!isspace(cur) && cur != '\0') {
        token[len] = cur;
        cur=source[++(*pos)];
        len++;
    }
    if(len == 0) {
        return NULL;
    }
    token[len] = '\0';
    //printf("%s %llu\n", token, strlen(token));
    return token;
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
        int len = strlen(token);
//      // Check if it's a label definition
        if (token[0] == ':') {
            char* tokname = strdup(token) + 1; 
            labels_write(a, token);
            free(token);
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
                char* tokname = token+1;
                Label* addr = labels_find(&a->labels, tokname);
                if(!addr) {
                    printf("Could not find label");
                }
                //int constant = add_constant(a, lobj_func(addr));
                write_byte(a, 0);
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
            token = read_token(source, &pos);
            write_byte(a, parse_number(token));
        } else if (strcmp(token, "GET_LOCAL") == 0) {
            write_byte(a, I_GET_LOCAL);
            token = read_token(source, &pos);
            write_byte(a, parse_number(token));
        } else if (strcmp(token, "SET_LOCAL") == 0) {
            write_byte(a, I_SET_LOCAL);
            token = read_token(source, &pos);
            write_byte(a, parse_number(token));
        } else if (strcmp(token, "JMP") == 0) {
            write_byte(a, I_JMP);
            token = read_token(source, &pos);
            int offset = parse_number(token);
            write_short(a, offset);
        } else if (strcmp(token, "JMP_FALSY") == 0) {
            write_byte(a, I_JMP_FALSY);
            token = read_token(source, &pos);
            int offset = parse_number(token);
            write_short(a, offset);
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
    vm_function* fn = ALLOCATE(vm_function, 1);
    fn->obj.type = OBJ_FUNC;  // Function type
    fn->arity = 0;  // Default to 0 arguments
    Label* l = labels_find(&a->labels, "main"); 
    if(l == NULL) {
        printf("Could not find entrypoint?\n");
        exit(0);
    }
    fn->chunk = l->code;
    fn->chunk_ct = l->count;

    fn->values = l->constants;
    fn->values_ct = l->constants_count;
    fn->name = lobj_str("__main__");
    return (lobj){ 
        .type=LVAL_OBJ,
        .v.obj=(vm_object*)fn
    };
}

void asm_free(Assembler* a) {
    // Free label names
    darray_free(a->labels);
}

int main() {
    const char* source = 
    "; Simple program to add two numbers (3 + 4)\n"             \
    "; Result should be 7                       \n"             \
    ":main                                     \n"             \
    "   LOAD 3          ; Push first number (3)\n"              \
    "   LOAD 4          ; Push second number (4)\n"             \
    "   ADD             ; Add numbers together\n"               \
    "   HALT            ; Stop program and show result\n";      

    Assembler assembler;
    asm_init(&assembler);
    asm_compile(&assembler, source);
    
    VM vm;
    HashTable globals, strings;
    table_create(&globals);
    table_create(&strings);

    lobj main = asm_get_entry(&assembler);
    //todo: init
    //apply main function
    
    vm_create(&vm, &globals, &strings, &main);
    vm_exec(&vm, 0);

    
}


