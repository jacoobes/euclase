#ifndef vm_h
#define vm_h

#define MAX_LOCALS 256
#define FRAMES_MAX 256
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_STR, LVAL_OBJ };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
enum { OBJ_STR, OBJ_FUNC, OBJ_USR };

enum Instruction {
    I_HALT        , // End execution 
    I_LOAD        , // Load value at index to stack.
    I_NEGATE      , // negate a number
    I_ADD         , // add two values
    I_SUB         , // subtract two numbers
    I_MUL         , // multiply two numbers
    I_DIV         , // divide two numbers
    I_MOD         , // modulus two numbers
    I_CALL        , // call function
    I_JMP         , // jmp to instr ptr
    I_GET_LOCAL   ,
    I_SET_LOCAL   ,
    I_POP,
    I_RETURN
};

typedef struct {
   int type; 
} vm_object;

typedef union {
    double num;
    int err;
    vm_object* obj;
} Value;

/* Declare New lval Struct */
typedef struct {
  int type;
  Value v;
} lobj; //lisp object

typedef struct {
  vm_object obj;
  int arity;

  uint8_t* chunk;
  uint8_t chunk_ct;

  lobj* values;
  int values_ct;
  lobj name;
} vm_function;

typedef struct {
    vm_object obj;
    int len; 
    char* contents; //must be null terminated
} vm_string;


typedef struct {
    uint8_t* ip;
    vm_function* function;
    lobj* slots; //the reference to the stack that the function can use
} CallFrame;



#define DEFAULT_CALL_STACK_SIZE 256
#define DEFAULT_NUM_LOCALS 10



typedef struct {
    // global variable space
    int *globals;
    int nglobals;

    // Operand stack, grows upwards
    lobj stack[256];

    //stack pointer. This points 1 PAST the top of the stack
    //I remember it like this:
    //stackTop points to where the next value to be pushed will go.
    //The maximum number of values we can store on the stack (for now, at least) is 256
    lobj* stack_top;
    
    CallFrame call_stack[DEFAULT_CALL_STACK_SIZE];
    int frame_count;
    
} VM;


typedef struct {
    vm_string name;
    int depth;
} Local;

typedef struct {

    uint8_t* code;
    int code_cap;
    int code_count;

    lobj* values; 
    int values_count;
    int val_cap;


    Local locals[MAX_LOCALS]; //subject to change
    int scope_depth;
    int local_count; 

} Compiler;

lobj lobj_str(char *, int len);
lobj lobj_num(double x);
lobj lobj_func(uint8_t* initial_code, uint8_t code_ct, lobj* values, int values_count, uint8_t arity, lobj name);
lobj lobj_err(int err);
void lobj_free(lobj*);
void compiler_create(Compiler*);
int  compiler_add_constant(Compiler*, lobj);
void compiler_emit(Compiler* c, uint8_t byte);
void compiler_free(Compiler*);
void vm_create(VM* vm,
            int *globals, //globals
            int nglobals, //num globals
            lobj* main); 

void vm_exec(VM*, int);

#endif
