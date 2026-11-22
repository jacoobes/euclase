#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../reader.h"
#include "../compiler.h"
#include "../table.h"
#include "../vm.h"
#include "../darray.h"

// Compile and run `src` the way main.c does, then return the number stored in
// the global `r`. Programs end with a bare `r` so the stack is not left empty.
static double run_and_get_r(const char* src) {
    size_t count;
    SExpr** exprs = read_all(src, &count);
    static HashTable globals, strings;
    table_create(&globals);
    table_create(&strings);
    Compiler compiler;
    compiler_create(&compiler, &globals, &strings);
    for (size_t i = 0; i < count; i++) compiler_emit(&compiler, exprs[i]);
    CompileUnit* main_fn = compiler.current_unit;
    darray_push(main_fn->bytes, I_RETURN);
    lobj main = compile_unit_get_closure(main_fn);
    compiler_destroy(&compiler);

    static VM vm;
    vm_create(&vm, &globals, &strings);
    assert(vm_exec(&vm, &main, 0) == 0);

    vm_string* key = table_find_string(&strings, "r", hash_str("r", 1));
    assert(key != NULL);
    lobj v;
    assert(table_get(&globals, key, &v) == 1);
    assert(IS_NUM(v));
    return AS_NUM(v);
}

// A name bound twice in one let: the later binding overwrites the earlier one
// (issue #9). Previously this emitted I_SET_LOCAL 255 and corrupted the VM.
void test_let_duplicate_binding() {
    assert(run_and_get_r("(var r (let (a 1 a 2) a)) r") == 2);
    // the later right-hand side sees the earlier value
    assert(run_and_get_r("(var r (let (a 1 a (+ a 10)) a)) r") == 11);
    // locals bound after the duplicate still get the right slots
    assert(run_and_get_r("(var r (let (a 1 a 2 b 5) (+ a b))) r") == 7);
    // inside a function frame, where slot 255 used to overwrite vm->stack_top
    assert(run_and_get_r("(fn (f ()) (let (a 1 a 2 b 5 a 3) (+ a b))) (var r (f)) r") == 8);
    // unchanged: distinct names
    assert(run_and_get_r("(var r (let (a 1 b 2) (+ a b))) r") == 3);
    printf("test_let_duplicate_binding passed!\n");
}

int main() {
    test_let_duplicate_binding();
    printf("All tests passed!\n");
    return 0;
}
