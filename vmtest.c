#include "./vm.h"
#include <stdint.h>

//uint8_t code [] = {
//    I_LOAD, 0,
//    I_LOAD, 1,
//    I_ADD,
//    I_SET_LOCAL, 0,
//    I_GET_LOCAL, 0,
//    I_POP,
//    I_HALT
//};

int main() {
    lobj values[]={ lobj_num(1),
                    lobj_num(5), };
    VM vm;
    vm_create(&vm, 
              NULL, 0,
              values, sizeof(values) / sizeof(lobj), 0, 0);
    vm_exec(&vm, 0);

}
