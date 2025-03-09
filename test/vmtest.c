#include "../vm.h"
#include <stdint.h>
#include <stdio.h>


static uint8_t ADD_ARITY = 2;
static uint8_t add_code [] = {
        I_ADD,
        I_RETURN
};

uint8_t main_arity = 0;

static uint8_t main_code[] = {
        I_LOAD, 0, //we have to load 0 + 1
        I_LOAD, 1,
        I_LOAD, 2,
        I_CALL, 2, // I_CALL with argc 2 
        I_RETURN,
};
int main() {
    
    lobj add_fn_values[1];
    // create function called add
    vm_function* add = vm_function_create(add_code, sizeof(add_code),
                                          add_fn_values, sizeof(add_fn_values) / sizeof(lobj),
                                          ADD_ARITY, lobj_str("add"), 0);
    lobj add_closure = into_lobj(LVAL_OBJ, vm_closure_create(add));
    // the main function should have 
    lobj main_fn_values[]={add_closure, lobj_num(4), lobj_num(5)};
    VM vm;
    HashTable globals, strings;
    table_create(&globals);
    table_create(&strings);

    vm_function* main = vm_function_create(main_code, sizeof(main_code),
                                           main_fn_values, sizeof(main_fn_values) / sizeof(lobj),
                                           main_arity, lobj_str("__main__"), 0);
    lobj main_closure = into_lobj(LVAL_OBJ, vm_closure_create(main));
    //todo: init
    //apply main function
    
    vm_create(&vm, &globals, &strings, &main_closure);
    vm_exec(&vm, 0);
    return 0;

}
