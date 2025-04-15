#include "compiler.h"
#include <stdio.h>
#include "vm.h"


void local_init(Local* lc, const solid_str name, const int depth, const int reg) {
    lc->depth = depth;
    lc->reg=reg;
    lc->is_captured=false;
    lc->name=name;
}

Local local_create(const solid_str name, const int depth, const int reg) {
    Local ret;
    local_init(&ret, name, depth, reg);
    return ret;
}
void local_free(Local* lc) {
    solid_str_free(&lc->name);
    free(lc);
}



void localchart_init(LocalChart* lc) {
    if (lc==NULL) {
        printf("localchart_init: NULL\n");
        exit(1);
    }
    lc->cur_depth = 0;
    darray_init(lc->locals);
}
LocalChart localchart_create() {
    LocalChart lc;

    localchart_init(&lc);
    return lc;
}

void localchart_free(LocalChart* lc) {
    lc->cur_depth = 0;

    Local* l;
    darray_foreach(l, lc->locals) {
        solid_str_free(&l->name);
    }
    darray_free(lc->locals);
    free(lc);
}

int localchart_resolve_symbol(const LocalChart* lc, const char* symbol) {
    int found = -1;

    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth <= lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            found = local->reg;
        }
    }
    //printf("Found %d\n", found);
    return found;
}
int localchart_insert(LocalChart *lc, const char* symbol) {
    if(darray_size(lc->locals) == 256) {
        fprintf(stderr, "Variable overflow. Reached maximum variables per scope (256)");
        exit(1);
    }
    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth == lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            return -1;
        }
    }
    const int prev_size = darray_size(lc->locals);
    const Local new_local = local_create(solid_str_new(symbol), lc->cur_depth, prev_size);
    darray_push(lc->locals, new_local);
    return prev_size;
}
// name is duplicated, need to manage memory
CompileUnit* compileunit_create(const char* name, const int arity) {
    LocalChart* lc = malloc(sizeof(LocalChart));
    localchart_init(lc);
    // add function name to the 0th slot in locals
    darray_push(lc->locals, local_create(solid_str_new(name), 0, 0));

    CompileUnit* cu = malloc(sizeof(CompileUnit));
    darray_init(cu->bytes);
    darray_init(cu->constants);

    //duplicate the string name
    cu->name=strdup(name);

    cu->arity=arity;
    cu->locals_chart = lc;

    cu->upvalue_count = 0;
    cu->enclosing=NULL;
    return cu;
}


void compileunit_free(CompileUnit* cu) {
    if (!cu) return;
    free(cu->name);

    // NOTE:
    // VM takes the ownership of compile unit, so we don't need to free the compiler units
    localchart_free(cu->locals_chart);
}

lobj compile_unit_get_closure(CompileUnit* cu) {
    if (!cu) {
        printf("compile_unit_get_fn: NULL\n");
        exit(1);
    };
    //this is reallying ugly lol
    return into_lobj(LVAL_OBJ, vm_closure_create(vm_function_create(cu->bytes.item, darray_size(cu->bytes),
                                                          cu->constants.item, darray_size(cu->constants),
                                                          cu->arity, lobj_str(cu->name),
                                                          cu->upvalue_count)));
}


lobj compile_unit_get_fn(const CompileUnit* cu) {
    if (!cu) {
        printf("compile_unit_get_fn: NULL\n");
        exit(1);
    };
    //this is reallying ugly lol
    return into_lobj(LVAL_OBJ, vm_function_create(cu->bytes.item, darray_size(cu->bytes),
                                                          cu->constants.item, darray_size(cu->constants),
                                                          cu->arity, lobj_str(cu->name),
                                                          cu->upvalue_count));
}


void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings) {
    // darray_init(compiler->units);
    compiler->current_unit= compileunit_create("__main__", 0);

    compiler->globals=globals;
    compiler->strings=strings;

}

void compiler_destroy(Compiler *compiler) {
    while (compiler->current_unit->enclosing != NULL) {
        compileunit_free(compiler->current_unit);
        compiler->current_unit= compiler->current_unit->enclosing;
    }
}


int compiler_write_byte(const Compiler *compiler, const uint8_t byte) {
    CompileUnit* cur_unit = compiler->current_unit;
    darray_push(cur_unit->bytes, byte);
    return darray_size(cur_unit->bytes)-1;
}

// adds value to current units' compiler pool. Returns the size of array after pushing
int compiler_add_const(const Compiler *compiler, const lobj value) {
    CompileUnit* cu = compiler->current_unit;
    if (IS_STR(value)) {
        vm_string* str = AS_STR(value);
        const vm_string* dupe = table_find_string(compiler->strings, &str->contents, str->hash);

        // if dupe doesn't exist (it's new)
        if(!dupe) {
            darray_push(cu->constants, value);
            table_set(compiler->strings, str, lobj_num(1));
            return darray_size(cu->constants);
        }
        darray_push(cu->constants, into_lobj(LVAL_OBJ, (void*)dupe));
        vm_string_free(str);
        return darray_size(cu->constants);
    }
    darray_push(cu->constants, value);
    return darray_size(cu->constants);
}

// emits I_LOAD and adds constant to current unit's compiler pool
void compiler_load_const(const Compiler* compiler, lobj constant) {
    compiler_write_byte(compiler, I_LOAD);
#ifdef DEBUG
    printf("I_LOAD "); lobj_print(&constant); printf(" to %s\n", compiler->current_unit->name);
#endif


    compiler_add_const(compiler, constant);
    compiler_write_byte(compiler, darray_size(compiler->current_unit->constants) - 1);
}

size_t compiler_leave_scope(Compiler* compiler) {
    LocalChart* lc = compiler->current_unit->locals_chart;
    if(lc->cur_depth == 0) {
        return 0;
    }

    lc->cur_depth--;
    int removed = 0;
    while(lc->locals.size != 0) {
        const Local* cur = &(darray_item(lc->locals, darray_size(lc->locals)-1));
        if(cur->depth <= lc->cur_depth) break;
        darray_pop(lc->locals);
        removed++;
    }
    printf("leaving scope\n");
    return removed;
}
void compiler_enter_scope(Compiler* compiler) {
    compiler->current_unit->locals_chart->cur_depth++;
}

