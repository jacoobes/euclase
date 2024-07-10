#include "./vm.h"
#include <stdint.h>



int main() {


    uint8_t add_code [] = {
        I_LOAD, 0,
        I_LOAD, 1,
        I_ADD,
        I_RETURN
    };
    
    lobj add_fn_values[]={lobj_num(10), lobj_num(5)};
    lobj add = lobj_func(add_code, sizeof(add_code),
            add_fn_values, sizeof(add_fn_values) / sizeof(lobj));
    

    lobj main_fn_values[]={add};
    VM vm;
    uint8_t main_code [] = {
        I_CALL, 2,
        I_HALT
    };
    lobj main = lobj_func(main_code,
                         sizeof(main_code),
                         main_fn_values,
                         sizeof(main_fn_values) / sizeof(lobj));
    
        

    vm_create(&vm, 
              NULL, 0,
              &main);

    vm_exec(&vm, 0);
    

}
