#include "./vm.h"
#include <stdint.h>



int main() {

    uint8_t add_code [] = {
        I_LOAD, 0,
        I_RETURN
    };
    
    lobj add_fn_values[]={lobj_num(10)};
    lobj add = lobj_func(add_code, sizeof(add_code),
            add_fn_values, sizeof(add_fn_values) / sizeof(lobj), 0);
    

    lobj main_fn_values[]={add};
    VM vm;
    uint8_t main_code[] = {
        I_LOAD, 1,
        I_CALL, 0, 
        I_RETURN,
    };

    lobj main = lobj_func(main_code, sizeof(main_code),
                         main_fn_values, sizeof(main_fn_values) / sizeof(lobj),
                         0);

    vm_create(&vm, 
              NULL, 0,
              &main);
    //apply main function

    vm_exec(&vm, 0);
    

}
