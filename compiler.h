#ifndef compiler_h
#define compiler_h

#include "vm.h"
#include "reader.h"

#define UINT8_COUNT (UINT8_MAX + 1)
// most arguments a function call accepts; `and` uses the same operand limit
#define MAX_ARGS 256


/**
 * An value which that is in the stack frame. 
 */
typedef struct Local {
    solid_str name;
    // the depth is the scope of where the local value had been declared.
    // how many scopes were open when this local was declared;
    // leaving a scope drops every local deeper than the new LocalChart.cur_depth
    size_t depth;
    // the local's value lives, counted from the frame's base
    // can we squish this to a byte? need to bound check locals insert check to ensure
    // localchart_insert rejects slots above UINT8_MAX
    int slot;
    bool is_captured;
} Local;


typedef struct LocalChart {
    // Tracks the current depth of 
    int cur_depth;
    darray(Local) locals;
} LocalChart;

typedef struct  { 
    uint8_t index;
    bool is_local;
} Upvalue;

typedef struct CompileUnit CompileUnit;

// Compile-time constant. The compiler never allocates VM objects: strings are
// indexes into Compiler.strings and nested functions stay CompileUnits until
// compiler_link turns them into vm objects.
typedef struct {
    enum { K_NUM, K_STR, K_FN } tag;
    union {
        double num;
        int str;          // index into Compiler.strings
        CompileUnit* fn;  // nested function, owned by this constant
    } as;
} Const;

struct CompileUnit {
    darray(uint8_t) bytes;
    darray(Const) constants;

    // upvalues are closed over values that outlive the stack. They point to the index in the stack
    Upvalue upvalues[UINT8_COUNT];
    int upvalue_count;

    LocalChart* locals_chart;
    //owned: need to free this
    char* name;
    int arity;

    // values the VM will have in this frame at the current emit point, slot 0
    // (the callee) included. A new local's slot is stack_depth - 1.
    int stack_depth;

    struct CompileUnit* enclosing;

};

// name is duplicated, need to manage memory
CompileUnit* compileunit_create(CompileUnit* enclosing, const char* name, const int arity); 
void compileunit_free(CompileUnit* cu); 


typedef struct {
    // current_unit belongs to units, so when we clean up, we set to NULL, and then just free units
    CompileUnit* current_unit;
    // interned strings, deduplicated; Const.as.str indexes into this
    darray(solid_str) strings;
} Compiler;



void compiler_create(Compiler *compiler); 

void compiler_destroy(Compiler *compiler); 

int compiler_write_byte(const Compiler *compiler, const uint8_t byte); 

// adds value to current units' compiler pool. Returns the size of array after pushing
int compiler_add_const(const Compiler *compiler, const Const value); 

// emits I_LOAD and adds constant to current unit's compiler pool
void compiler_load_const(const Compiler* compiler, Const constant); 
size_t compiler_leave_scope(Compiler* lc); 
void compiler_enter_scope(const Compiler* lc);

int compiler_jump_emit(const Compiler* compiler, const enum Instruction instruction);

int localchart_resolve_symbol(const LocalChart* lc, const char* symbol);
int localchart_insert(LocalChart *lc, const char* symbol, int slot);
// writes an opcode and applies its stack effect to current_unit->stack_depth
void compiler_emit_op(const Compiler* compiler, enum Instruction op);

void compiler_jump_patch(const Compiler* compiler, size_t ushort_begin, size_t jump_loc);
void compileunit_write_ushort(CompileUnit* cur_unit, const unsigned short jump_loc);

// returns the index of symbol in compiler->strings, adding it if new
int compiler_fetch_string(Compiler* compiler, const char* symbol);


void compiler_emit(Compiler* compiler, SExpr* sexpr);

// Turns the compiled program into a VM closure: interns strings into
// vm->strings and builds vm_functions. Call once, after all compiler_emit calls.
lobj compiler_link(Compiler* compiler, VM* vm);

#endif
