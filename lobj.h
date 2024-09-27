#ifndef lobj_h
#define lobj_h

#include <stdint.h>

typedef enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_OBJ } LVAL;
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
typedef enum { OBJ_STR, OBJ_FUNC, OBJ_USR } LOBJ_TYPE;

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

typedef struct {
    vm_object obj;
    int len; 
    uint32_t hash;
    char* contents; //must be null terminated
} vm_string;


#define IS_NUM(obj) ((obj).type == LVAL_NUM)
#define IS_OBJ(obj) ((obj).type == LVAL_OBJ)
#define IS_FUNC(obj) (IS_OBJ((obj)) && (obj).v.obj->type == OBJ_FUNC)
#define IS_STR(obj) (IS_OBJ((obj)) && (obj).v.obj->type == OBJ_STR)

#define AS_STR(obj_) ((vm_string*)(obj_).v.obj)
#define AS_FUNC(obj_) ((vm_function*)(obj_).v.obj)
#define AS_NUM(obj_) ((obj_).v.num)

lobj lobj_str(char *);
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

