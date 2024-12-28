#ifndef lobj_h
#define lobj_h

#include <stdint.h>
#include "./solid_str.h"
#include <stdbool.h>

typedef enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_OBJ } LVAL;
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
typedef enum { OBJ_CLOSURE, OBJ_STR, OBJ_FUNC, OBJ_USR } LOBJ_TYPE;

// the base object class
typedef struct {
   LOBJ_TYPE type; 
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

typedef struct {
  vm_object obj;
  int arity;

  uint8_t* chunk;
  uint8_t chunk_ct;

  lobj* values;
  int values_ct;
  lobj name;
} vm_function;

//immutable wrapper for solid_str which extends a vm_obj
typedef struct {
    vm_object obj;
    unsigned long hash;
    solid_str contents;
} vm_string;

typedef struct {
    vm_object obj;
    vm_function* fn;
} vm_closure;



#define IS_NUM(obj) ((obj).type == LVAL_NUM)
#define IS_OBJ(obj) ((obj).type == LVAL_OBJ)
#define IS_FUNC(obj) (IS_OBJ((obj)) && (obj).v.obj->type == OBJ_FUNC)

#define AS_STR(obj_) ((vm_string*)(obj_).v.obj)
#define AS_FUNC(obj_) ((vm_function*)(obj_).v.obj)
#define AS_NUM(obj_) ((obj_).v.num)
#define OBJ_VAL(obj_) ((lobj){ .type=LVAL_OBJ, .v.obj=(vm_object*)(obj_) })

static inline bool is_closure(lobj* lval) {
    return (lval->type == LVAL_OBJ) 
    && (lval->v.obj->type == OBJ_CLOSURE);
}
static inline vm_closure* as_closure(lobj* lval) {
    return (vm_closure*)lval->v.obj;
}
static inline bool is_str(lobj* lval) {
    return lval->type == LVAL_OBJ && lval->v.obj->type == OBJ_STR;
}

static inline lobj into_lobj(LVAL ty, void* lval) {
    lobj lo =  (lobj){ 
        .type=ty,
        .v.obj=((vm_object*)lval)
    };
    return lo;
};
static inline lobj lobj_num(double x) {
   return (lobj){.type=LVAL_NUM, .v.num = x };
}

vm_function* vm_function_create(uint8_t* initial_code,
                                uint8_t code_ct,
                                lobj* values,
                                int values_count,
                                uint8_t arity,
                                lobj name);

vm_closure* vm_closure_create(vm_function* fn);

lobj lobj_str(const char *);
lobj lobj_num(double x);
lobj lobj_func(uint8_t* initial_code,
               uint8_t code_ct,
               lobj* values,
               int values_count,
               uint8_t arity,
               lobj name);
lobj lobj_err(int err);
void lobj_free(lobj*);
void lobj_print(lobj*);
void lobj_println(lobj*);
#endif

