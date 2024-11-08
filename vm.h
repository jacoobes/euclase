#ifndef vm_h
#define vm_h

#define MAX_LOCALS 256
#define FRAMES_MAX 256

#define TRUE 1
#define FALSE 0

#include <stdint.h>
#include "./table.h"


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
    I_JMP_FALSY   , // jmp if falsy
    I_GET_LOCAL   ,
    I_SET_LOCAL   ,
    I_POP,
    I_RETURN
};


typedef struct {
    uint8_t* ip;
    vm_function* function;
    lobj* slots; //the reference to the stack that the function can use
} CallFrame;



#define DEFAULT_CALL_STACK_SIZE 256
#define DEFAULT_NUM_LOCALS 10



typedef struct {
    // global variable space
    HashTable globals;
    HashTable strings;
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

/**
 * Initializes a new virtual machine instance and prepares it to execute the given main function.
 *
 * @param vm A pointer to the virtual machine (VM) instance to be initialized.
 * @param main A pointer to the main function (represented as an lobj) to be executed by the VM.
 */
void vm_create(VM* vm, lobj* main);

/**
 * Executes the virtual machine with the given configuration and flags.
 *
 * @param vm A pointer to the virtual machine instance to be executed.
 * @param flags An integer representing the flags used for customizing the execution of the virtual machine.
 */
void vm_exec(VM*, int);

/**
 * Frees the memory associated with the given virtual machine (VM) instance.
 * This function should be called to properly release the resources allocated
 * by the VM.
 *
 * @param vm Pointer to the VM instance to be freed.
 */
void vm_free(VM* vm);
#endif //vm_h
