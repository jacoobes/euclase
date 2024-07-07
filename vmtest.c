#include "./vm.h"
#include <stdint.h>

uint8_t code [] = {
    I_LOAD, 0,
    I_LOAD, 1,
    I_ADD,
    I_HALT
};

int main() {
    lobj values[]={ lobj_num(1),
                    lobj_num(5), };
    VM vm;
    lobj main = lobj_func( code, sizeof(code), values, sizeof(values) / sizeof(lobj));
    vm_create(&vm, 
              NULL, 0,
              &main);

    vm_exec(&vm, 0);
    

}
