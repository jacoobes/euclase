#ifndef vm_h
#define vm_h

#include "solid_str.h"
#define MAX_LOCALS 256
#define FRAMES_MAX 256
#include <stdbool.h>
#include <stdint.h>

// infrastructure:
// stack based machine
// 

typedef struct VM VM;

// declared in table.h
typedef struct HashTable HashTable;
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


typedef enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_OBJ } LVAL;
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
typedef enum {
    OBJ_CLOSURE,
    OBJ_STR,
    OBJ_FUNC,
    OBJ_UPVALUE,
    OBJ_NATIVE,
    OBJ_USR 
} LOBJ_TYPE;


// the base object class
typedef struct {
   LOBJ_TYPE type; 
   bool is_marked;
} vm_object;

// a union of all possible virtual machine values
typedef union {
    double num;
    int err;
    vm_object* obj;
} Value;

/* Declare New lval Struct */
// discriminated union. int is the type of 
typedef struct {
  LVAL type;
  Value v;
} lobj; //lisp object

typedef struct vm_upvalue {
    vm_object obj;
    lobj* location;
    lobj closed;
    struct vm_upvalue *next;
} vm_upvalue;

typedef struct {
    vm_object obj;
    int arity;

    uint8_t* chunk;
    uint8_t chunk_ct;

    lobj* values;
    int values_ct;

    lobj name;
    int upvalues_count;
} vm_function;

typedef struct {
    vm_object obj;
    vm_upvalue** upvalues; //maximum upvalue count
    uint8_t upvalues_ct;
    vm_function* fn;
} vm_closure;

//immutable wrapper for solid_str which extends a vm_obj
typedef struct vm_string {
    vm_object obj;
    solid_str contents;
} vm_string;

typedef lobj (*native_func)(int argc, lobj* args);
typedef struct vm_native {
    vm_object obj;
    int argc;
    native_func native;
} vm_native;



typedef struct {
    uint8_t* ip;
    // a pointer to a closure, we dont need to free this.
    vm_closure* closure;
    // we do not need to free this: points to somewhere on the stack
    lobj* slots; //the reference to the stack that the function can use

} CallFrame;





#define DEFAULT_CALL_STACK_SIZE 256
#define DEFAULT_NUM_LOCALS 10



typedef struct VM {
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


    int gray_count;
    int gray_capacity;
    vm_object** gray_stack;
    
    

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



#define IS_NUM(obj) ((obj).type == LVAL_NUM)
#define IS_OBJ(obj) ((obj).type == LVAL_OBJ)
#define IS_FUNC(obj) (IS_OBJ((obj)) && (obj).v.obj->type == OBJ_FUNC)
#define IS_STR(obj_) (IS_OBJ((obj_)) && (obj_).v.obj->type == OBJ_STR)

#define AS_STR(obj_) ((vm_string*)(obj_).v.obj)
#define AS_FUNC(obj_) ((vm_function*)(obj_).v.obj)
#define AS_NUM(obj_) ((obj_).v.num)

static inline bool is_closure(lobj* lval) {
    return (lval->type == LVAL_OBJ)  && (lval->v.obj->type == OBJ_CLOSURE);
}
static inline vm_closure* as_closure(lobj* lval) {
    return (vm_closure*)lval->v.obj;
}

static inline bool is_native(lobj* lval) {
    return (lval->type == LVAL_OBJ) && (lval->v.obj->type == OBJ_NATIVE);
}

static inline vm_native* as_native(lobj* lval ) {
    return (vm_native*)lval->v.obj;
}


static inline bool is_str(lobj* lval) {
    return lval->type == LVAL_OBJ && lval->v.obj->type == OBJ_STR;
}

static inline lobj into_lobj(void* l_val) {
    return (const lobj){
        .type=LVAL_OBJ,
        .v.obj=((vm_object*)l_val)
    };
};

static inline lobj lobj_num(double x) {
   return (const lobj){.type=LVAL_NUM,
                       .v.num = x };
}



vm_function* vm_function_create(uint8_t* initial_code,
                                uint8_t code_ct,
                                lobj* values,
                                int values_count,
                                uint8_t arity,
                                lobj name,
                                int upvalues_count);
vm_native* vm_native_create(
    int argc,
    native_func fn
);

vm_closure* vm_closure_create( vm_function* fn);
vm_string* vm_string_create( const char* contents);
void vm_string_free(vm_string* s);
vm_upvalue* vm_upvalue_create(lobj* location);
lobj lobj_str( const char *);
lobj lobj_num(double x);
lobj lobj_err(int err);
void lobj_free(lobj*);
void lobj_print(lobj*);
void lobj_println(lobj*);



#endif //vm_h
