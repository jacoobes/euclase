#ifndef vm_h
#define vm_h

#define MAX_LOCALS 256
#define FRAMES_MAX 256


#include <stdint.h>
#include "./table.h"


// infrastructure:
// stack based machine
// 


enum Instruction {
    I_HALT        , // End execution 
    I_LOAD        , // Load value at index to stack.
    I_NEGATE      , // negate a number
    I_ADD         , // add two values
    I_SUB         , // subtract two numbers
    I_MUL         , // multiply two numbers
    I_DIV         , // divide two numbers
    I_MOD         , // modulus two numbers
    I_LTE         , // LTE two numbers
    I_GT         , //  GTE two numbers
    I_CALL        , // call function
    I_JMP         , // jmp to instr ptr
    I_JMP_FALSY   , // jmp if falsy
    I_GET_LOCAL   , // Get local variable
    I_SET_LOCAL   , // SET LOCAL VARIABLE
    I_SET_GLOBAL  , // set global variable
    I_GET_GLOBAL  , // get global
    I_POP         , 
    I_RETURN      ,
    I_LOOP        ,
    I_CLOSURE     ,
    I_GET_UPVALUE ,
    I_SET_UPVALUE ,
    I_CLOSE_UPVALUE ,
};


typedef struct {
    uint8_t* ip;
    // a pointer to a closure, we dont need to free this.
    vm_closure* closure;
    // we do not need to free this: points to somewhere on the stack
    lobj* slots; //the reference to the stack that the function can use

} CallFrame;

#define DEFAULT_CALL_STACK_SIZE 256
#define DEFAULT_NUM_LOCALS 10



typedef struct {
    // global variable space
    HashTable* globals;
    HashTable* strings;
    // Operand stack, grows upwards
    lobj stack[256];

    //stack pointer. This points 1 PAST the top of the stack
    //I remember it like this:
    //stack_top points to where the next value to be pushed will go.
    //The maximum number of values we can store on the stack (for now, at least) is 256
    lobj* stack_top;
    
    // call stack: functions 
    CallFrame call_stack[DEFAULT_CALL_STACK_SIZE];
    vm_upvalue* open_upvalues;
    vm_closure* main_fn;
    int frame_count;
} VM;




/**
 * Initializes a new virtual machine instance and prepares it to execute the given main function.
 *
 * @param vm A pointer to the virtual machine (VM) instance to be initialized.
 * @param globals
 * @param strings
 * @param main A pointer to the main function (represented as an lobj) to be executed by the VM.
 */
void vm_create(VM* vm, HashTable* globals, HashTable* strings, lobj* main);

/**
 * Executes the virtual machine with the given configuration and flags.
 *
 * @param vm A pointer to the virtual machine instance to be executed.
 * @param flags An integer representing the flags used for customizing the execution of the virtual machine.
 */
int vm_exec(VM*, int);

/**
 * Frees the memory associated with the given virtual machine (VM) instance.
 * This function should be called to properly release the resources allocated
 * by the VM.
 *
 * @param vm Pointer to the VM instance to be freed.
 */
void vm_free(const VM* vm);


solid_str vm_stacktrace(VM* vm, char* error);

#endif //vm_h
