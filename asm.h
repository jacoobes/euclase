// asm.h
#ifndef ASM_H
#define ASM_H

#include <stdint.h>
#include "./vm.h"
#include "./darray.h"

typedef struct {
    char* name;
    int address;

    uint8_t* code;
    int capacity;
    int count;

    lobj* constants;
    int constants_count;
} Label;

typedef darray(Label*) LabelArray;

typedef struct {
    Label* current_label;
    LabelArray labels;
} Assembler;

void asm_init(Assembler* a);
void asm_free(Assembler* a);
int asm_compile(Assembler* a, const char* source);
lobj asm_get_entry(Assembler* a);

#endif
