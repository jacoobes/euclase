#ifndef compiler_h
#define compiler_h

#include "table.h"
#include "lobj.h"
#include "vm.h"

#define UINT8_COUNT (UINT8_MAX + 1)

typedef struct Local {
    solid_str name;
    size_t depth;
    //can we squish this to a byte? need to bound check locals insert check to ensure
    int reg;
    bool is_captured;
} Local;


typedef struct LocalChart {
    int cur_depth;
    darray(Local) locals;
} LocalChart;

typedef struct  { 
    uint8_t index;
    bool is_local;
} Upvalue;

typedef struct CompileUnit {
    darray(uint8_t) bytes;
    darray(lobj) constants;

    // upvalues are closed over values that outlive the stack. They point to the index in the stack
    Upvalue upvalues[UINT8_COUNT];
    int upvalue_count;

    LocalChart* locals_chart;
    //owned: need to free this
    char* name;
    int arity;
    struct CompileUnit* enclosing;
} CompileUnit;

// name is duplicated, need to manage memory
CompileUnit* compileunit_create(CompileUnit* enclosing, const char* name, const int arity); 
void compileunit_free(CompileUnit* cu); 
lobj compile_unit_get_closure(CompileUnit* cu); 
// a compile unit is transformed into a lobj of type OBJ with CLOSURE via copy
lobj compile_unit_get_fn(const CompileUnit* cu); 


typedef struct {
    // darray(CompileUnit*) units;
    // current_unit belongs to units, so when we clean up, we set to NULL, and then
    // just free units
    CompileUnit* current_unit;
    HashTable* globals;
    HashTable* strings;
} Compiler;



void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings); 

void compiler_destroy(Compiler *compiler); 

int compiler_write_byte(const Compiler *compiler, const uint8_t byte); 

// adds value to current units' compiler pool. Returns the size of array after pushing
int compiler_add_const(const Compiler *compiler, const lobj value); 

// emits I_LOAD and adds constant to current unit's compiler pool
void compiler_load_const(const Compiler* compiler, lobj constant); 
size_t compiler_leave_scope(Compiler* lc); 
void compiler_enter_scope(const Compiler* lc);

int compiler_jump_emit(const Compiler* compiler, const enum Instruction instruction);

int localchart_resolve_symbol(const LocalChart* lc, const char* symbol);
int localchart_insert(LocalChart *lc, const char* symbol);

void compiler_jump_patch(const Compiler* compiler, size_t ushort_begin, size_t jump_loc);
void compileunit_write_ushort(CompileUnit* cur_unit, const unsigned short jump_loc);
#endif

