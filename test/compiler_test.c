#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../reader.h"
#include "../compiler.h"
#include "../table.h"
#include "../vm.h"
#include "../gc.h"

// Compile `src` and link it into `vm` the way main.c does; returns main's closure.
static lobj compile_and_link(const char* src, VM* vm) {
    size_t count;
    SExpr** exprs = read_all(src, &count);
    Compiler compiler;
    compiler_create(&compiler);
    for (size_t i = 0; i < count; i++) compiler_emit(&compiler, exprs[i]);
    gc_init(vm);
    vm_create(vm);
    lobj main = compiler_link(&compiler, vm);
    compiler_destroy(&compiler);
    return main;
}

// Compile and run `src`, then return the number stored in the global `r`.
// Programs end with a bare `r` so the stack is not left empty.
static double run_and_get_r(const char* src) {
    static VM vm;
    lobj main = compile_and_link(src, &vm);
    assert(vm_exec(&vm, &main, 0) == 0);

    vm_string* key = table_find_string(vm.strings, "r", hash_str("r", 1));
    assert(key != NULL);
    lobj v;
    assert(table_get(vm.globals, key, &v) == 1);
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

// Compile `src` in a child process and return its wait status. Used for
// programs the compiler must reject with exit(1) rather than crash on.
static int compile_in_child(const char* src) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        size_t count;
        SExpr** exprs = read_all(src, &count);
        Compiler compiler;
        compiler_create(&compiler);
        for (size_t i = 0; i < count; i++) compiler_emit(&compiler, exprs[i]);
        _exit(0); // compiled without error
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return status;
}

static int rejected(const char* src) {
    int st = compile_in_child(src);
    return WIFEXITED(st) && WEXITSTATUS(st) == 1;
}

// `if` needs exactly a condition, a then-branch and an else-branch. Anything
// else must be a compile error, not a NULL dereference (issue #8).
void test_if_arity() {
    assert(rejected("(if)"));
    assert(rejected("(if 1)"));
    assert(rejected("(if 1 2)"));        // no else branch
    assert(rejected("(if 1 2 3 4)"));    // extra expression
    // well-formed if still compiles and picks the right branch
    int st = compile_in_child("(if 1 2 3)");
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    assert(run_and_get_r("(var r (if 1 10 20)) r") == 10);
    assert(run_and_get_r("(var r (if 0 10 20)) r") == 20);
    printf("test_if_arity passed!\n");
}

// Build "(var r (let (x 1) (and x x ... x L))) r" with n operands, the last
// one being the literal L. The operands read a local rather than repeating the
// literal 1, so the long form does not fill the constant pool (see #10).
static char* and_of_ones(int n, int last) {
    char* src = malloc(64 + 2 * n);
    char* p = src + sprintf(src, "(var r (let (x 1) (and");
    for (int i = 0; i < n - 1; i++) p += sprintf(p, " x");
    sprintf(p, " %d))) r", last);
    return src;
}

// `and` short-circuits: it yields the first falsy operand (0), otherwise the
// last operand. It takes 2 up to MAX_ARGS operands (the function call limit);
// any other count is a compile error (issue #8).
void test_and() {
    // wrong operand count: must be rejected, not crash
    assert(rejected("(and)"));
    assert(rejected("(and 1)"));
    // one more operand than the limit
    char* too_many = and_of_ones(MAX_ARGS + 1, 1);
    assert(rejected(too_many));
    free(too_many);

    // two operands
    assert(run_and_get_r("(var r (and 1 2)) r") == 2);
    assert(run_and_get_r("(var r (and 0 2)) r") == 0);
    assert(run_and_get_r("(var r (and 1 0)) r") == 0);
    assert(run_and_get_r("(var r (and 0 0)) r") == 0);
    printf("  and: two operands ok\n");

    // short-circuit: the second operand is not evaluated when the first is
    // falsy (calling the undefined `nope` would be a runtime error)
    assert(run_and_get_r("(var r (and 0 (nope))) r") == 0);
    printf("  and: short-circuit ok\n");

    // leaves exactly one value on the stack: as a call argument, an extra
    // value would shift the callee and argument slots
    assert(run_and_get_r("(fn (id (x)) x) (var r (id (and 1 2))) r") == 2);
    assert(run_and_get_r("(fn (id (x)) x) (var r (id (and 0 2))) r") == 0);
    printf("  and: stack balance ok\n");

    // more than two operands: every operand takes part
    assert(run_and_get_r("(var r (and 1 2 3)) r") == 3);
    assert(run_and_get_r("(var r (and 1 1 0)) r") == 0);
    assert(run_and_get_r("(var r (and 1 2 3 4)) r") == 4);
    assert(run_and_get_r("(var r (and 1 2 0 4)) r") == 0);
    printf("  and: several operands ok\n");

    // exactly the limit compiles and runs; a falsy last operand still counts
    char* at_limit = and_of_ones(MAX_ARGS, 1);
    assert(run_and_get_r(at_limit) == 1);
    free(at_limit);
    at_limit = and_of_ones(MAX_ARGS, 0);
    assert(run_and_get_r(at_limit) == 0);
    free(at_limit);
    printf("  and: %d operands ok\n", MAX_ARGS);

    printf("test_and passed!\n");
}

// `var` defines a global: (var name value). A missing name, a non-symbol
// name or a missing value must be a compile error, not a crash (issue #8).
void test_var() {
    // malformed: must be rejected, not crash
    assert(rejected("(var)"));
    assert(rejected("(var a)"));
    assert(rejected("(var 5 1)"));
    assert(rejected("(var \"a\" 1)"));
    assert(rejected("(var a 1 2)"));     // extra value
    printf("  var: malformed forms rejected\n");

    // well-formed
    assert(run_and_get_r("(var r 5) r") == 5);
    assert(run_and_get_r("(var r (+ 1 2)) r") == 3);
    assert(run_and_get_r("(var r (% 7 3)) r") == 1); // #40
    // redefining a global replaces its value
    assert(run_and_get_r("(var r 1) (var r 2) r") == 2);
    // the value can refer to another global
    assert(run_and_get_r("(var a 4) (var r (+ a 1)) r") == 5);
    printf("  var: well-formed ok\n");

    printf("test_var passed!\n");
}

// `let` binds locals: (let (name value ...) body...). Malformed binding lists
void test_let() {
    // malformed: must be rejected, not crash
    assert(rejected("(let)"));              // no bindings at all
    assert(rejected("(let (a) a)"));        // odd binding list, first pair
    assert(rejected("(let (a 1 b) a)"));    // odd binding list, later pair
    assert(rejected("(let (5 2) 1)"));      // name is not a symbol
    assert(rejected("(let a 1)"));          // bindings are not a list
    printf("  let: malformed forms rejected\n");

    // well-formed
    assert(run_and_get_r("(var r (let (a 1) a)) r") == 1);
    assert(run_and_get_r("(var r (let (a 1 b 2) (+ a b))) r") == 3);
    // a later binding can use an earlier one
    assert(run_and_get_r("(var r (let (a 2 b (+ a 3)) b)) r") == 5);
    // nested lets, and the body's last form is the value
    assert(run_and_get_r("(var r (let (a 1) (let (b 10) (+ a b)))) r") == 11);
    assert(run_and_get_r("(var r (let (a 1) 5 a)) r") == 1);
    printf("  let: well-formed ok\n");

    printf("test_let passed!\n");
}

// `while` loops: (while cond body...). A missing condition must be a compile
void test_while() {
    assert(rejected("(while)"));
    printf("  while: malformed form rejected\n");

    // sum 5 + 4 + 3 + 2 + 1
    assert(run_and_get_r("(var r (let (i 5  s 0) \
                                        (while i (@s (+ s i)) (@i (- i 1))) s)) r") == 15);
    // a false condition never runs the body
    assert(run_and_get_r("(var r (let (s 1) (while 0 (@s 99)) s)) r") == 1);
    printf("  while: well-formed ok\n");

    printf("test_while passed!\n");
}

// A loop must not grow the operand stack on every iteration. Today the body
// re-emits the condition and nothing pops the body's values, so a few hundred
// iterations overflow the 256-slot stack (issue #11). Runs last so it does not
// hide the results above.
void test_while_stack_balance() {
    assert(run_and_get_r("(var r (let (i 300) \
                                    (while i (@i (- i 1))) 7)) r") == 7);
    printf("test_while_stack_balance passed!\n");
}

// Compile and run `src` in a child process. Returns 1 if the program was
// refused, either by the compiler (exit 1) or with a runtime error (vm_exec
// returning non-zero); 0 if it ran cleanly; crashes fail the assert.
static int fails(const char* src) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        static VM vm;
        lobj main = compile_and_link(src, &vm);
        _exit(vm_exec(&vm, &main, 0) != 0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status)); // a crash is never an acceptable answer
    return WEXITSTATUS(status) == 1;
}

// `@name value` assigns to an existing local, upvalue or global and yields the
// value. A missing name or value must be a compile error, not a crash (#8).
void test_assign() {
    // malformed: must be rejected, not crash
    assert(rejected("(@)"));          // no name
    assert(rejected("(@a)"));         // no value
    assert(rejected("(@a 1 2)"));     // extra value
    printf("  @: malformed forms rejected\n");

    // assigning to a name that was never defined is an error, at compile time
    // or at run time, rather than silently creating a global
    assert(fails("(@nope 5)"));
    assert(fails("(fn (f ()) (@nope 5)) (f)"));
    printf("  @: unknown name rejected\n");

    // globals are immutable (README, "Reassigning local variables"), so @ on a
    // global is an error, at top level or from inside a function
    assert(fails("(var g 1) (@g 7)"));
    assert(fails("(var g 1) (fn (set ()) (@g 9)) (set)"));
    printf("  @: global assignment rejected\n");

    // locals
    assert(run_and_get_r("(var r (let (a 1) (@a 5) a)) r") == 5);
    assert(run_and_get_r("(var r (let (a 1) (@a (+ a 10)) a)) r") == 11);

    // upvalues: a closure keeps updating the variable it captured
    assert(run_and_get_r(
        "(fn (mk ()) (let (c 0) (fn (inc ()) (@c (+ c 1))) inc))"
        "(var i (mk)) (i) (var r (i)) r") == 2);
    printf("  @: locals and upvalues ok\n");

    printf("test_assign passed!\n");
}

// `@` is an expression: it yields the assigned value, for locals and for
// upvalues, so it can be used inside a larger expression.
void test_assign_value() {
    assert(run_and_get_r("(var r (let (a 1) (+ 1 (@a 5)))) r") == 6);
    assert(run_and_get_r(
        "(fn (mk ()) \
            (let (c 0) \
                (fn (inc ())  \
                    (+ 100 (@c (+ c 1)))) inc))"
        "(var i (mk)) (i) (var r (i)) r") == 102);
    printf("test_assign_value passed!\n");
}

// `fn` defines a function: (fn (name (args...)) body...), per the README.
// Every malformed shape must be a compile error, not a crash (issue #8).
void test_fn() {
    // malformed: must be rejected, not crash
    assert(rejected("(fn)"));                    // nothing after fn
    assert(rejected("(fn ())"));                 // empty definition, no name
    assert(rejected("(fn (f))"));                // no argument list
    assert(rejected("(fn f 1)"));                // definition is not a list
    assert(rejected("(fn (5 ()) 1)"));           // name is not a symbol
    assert(rejected("(fn (f a) a)"));            // arguments are not a list
    assert(rejected("(fn (f (a 5)) a)"));        // argument is not a symbol
    assert(rejected("(fn (f (a a)) a)"));        // duplicate argument
    assert(rejected("(fn (f () x) 1)"));         // extra part in the definition
    printf("  fn: malformed forms rejected\n");

    // well-formed
    assert(run_and_get_r("(fn (add (a b)) (+ a b)) (var r (add 10 20)) r") == 30);
    // recursion
    assert(run_and_get_r(
        "(fn (fact (x)) (if (<= x 2) x (* x (fact (- x 1)))))"
        "(var r (fact 5)) r") == 120);
    // an empty body returns 0, and the last body form is the value
    assert(run_and_get_r("(fn (f ())) (var r (f)) r") == 0);
    assert(run_and_get_r("(fn (f ()) 1 2 3) (var r (f)) r") == 3);
    // a function defined inside another one is local to it
    assert(run_and_get_r(
        "(fn (outer (x)) (fn (inner (y)) (* y 2)) (inner x))"
        "(var r (outer 21)) r") == 42);
    printf("  fn: well-formed ok\n");

    printf("test_fn passed!\n");
}

int main() {
    test_let_duplicate_binding();
    test_if_arity();
    test_and();
    test_var();
    test_let();
    test_while();
    test_while_stack_balance();
    test_assign();
    test_assign_value();
    test_fn();
    printf("All tests passed!\n");
    return 0;
}
