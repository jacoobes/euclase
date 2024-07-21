#include "./vm.h"
#include <stdint.h>



int main() {
    uint8_t add_arity = 2;
    uint8_t add_code [] = {
        I_ADD,
        I_RETURN
    };
    
    lobj add_fn_values[]={};
    lobj add = lobj_func(add_code, sizeof(add_code),
            add_fn_values, sizeof(add_fn_values) / sizeof(lobj),
            add_arity, lobj_str("add", 3));
    
    
    uint8_t main_arity = 0;
    lobj main_fn_values[]={add, lobj_num(4), lobj_num(5)};
    VM vm;
    uint8_t main_code[] = {
        I_LOAD, 0, //we have to load 0 + 1
        I_LOAD, 1,
        I_LOAD, 2,
        I_CALL, 2, 
        I_RETURN,
    };
    lobj main = lobj_func(main_code, sizeof(main_code),
                          main_fn_values, sizeof(main_fn_values) / sizeof(lobj),
                          main_arity, lobj_str("__main__", 8));

    vm_create(&vm, 
              NULL, 0,
              &main);
    //apply main function

    vm_exec(&vm, 0);
    

}
