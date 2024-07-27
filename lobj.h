#ifndef lobj_h
#define lobj_h

#include <stdint.h>

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_STR, LVAL_OBJ };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
enum { OBJ_STR, OBJ_FUNC, OBJ_USR };

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
    uint32_t hash;
    char* contents; //must be null terminated
} vm_string;


int IS_NUM(lobj l);
int IS_OBJ(lobj l);
lobj lobj_str(char *, int len);
lobj lobj_num(double x);
lobj lobj_func(uint8_t* initial_code, uint8_t code_ct, lobj* values, int values_count, uint8_t arity, lobj name);
lobj lobj_err(int err);
void lobj_free(lobj*);
void lobj_print(lobj);
void lobj_println(lobj);
#endif

