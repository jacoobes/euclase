#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../reader.h"

void test_read_number() {
    int pos = 0;
    SExpr *expr = read_number("123", 1, &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_NUMBER);
    assert(expr->value.number == 123);
    free_sexpr(expr);
    //pos = 0;
    // expr = read_number("-456", -1, &pos);
    // assert(expr != NULL);
    // assert(expr->type == SEXPR_NUMBER);
    // assert(expr->value.number == -456);
    // free_sexpr(expr);

    printf("test_read_number passed!\n");
}

void test_read_symbol() {
    int pos = 0;
    SExpr *expr = read_symbol("foo", &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_SYMBOL);
    assert(strcmp(expr->value.symbol, "foo") == 0);
    free_sexpr(expr);

    pos = 0;
    expr = read_symbol("true", &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_NUMBER);  // true is interpreted as a number (1)
    assert(expr->value.number == 1);
    free_sexpr(expr);

    pos = 0;
    expr = read_symbol("false", &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_NUMBER);  // false is interpreted as a number (0)
    assert(expr->value.number == 0);
    free_sexpr(expr);

    printf("test_read_symbol passed!\n");
}

void test_read_string() {
    int pos = 0;
    SExpr *expr = read_string("\"hello world\"", &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_STRING);
    assert(strcmp(expr->value.string, "hello world") == 0);
    free_sexpr(expr);

    pos = 0;
    expr = read_string("\"escaped \\\"string\\\"\"", &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_STRING);
    assert(strcmp(expr->value.string, "escaped \"string\"") == 0);
    free_sexpr(expr);

    printf("test_read_string passed!\n");
}

void test_read_list() {
    printf("Begin read list\n");
    int pos = 0;
    const char *input = "(+ 1 2)";
    SExpr *expr = read_list(input, &pos);
    SExpr *cur = expr; 
    // Check the outer list
    printf("expr is a LIST\n");
    assert(cur != NULL && cur->type == SEXPR_LIST);
    printf("done: expr is a LIST\n");
    
    // Check the + symbol
    printf("first is a SEXPR_SYMBOL\n");
    SExpr *plus = sexpr_list_first(cur);
    assert(plus != NULL && plus->type == SEXPR_SYMBOL);
    assert(strcmp(plus->value.symbol, "+") == 0);
    printf("done: first is a SEXPR_SYMBOL\n");


    printf("rest is a SEXPR_LIST\n");
    // Move to the 1
    cur = sexpr_list_rest(cur);
    assert(cur!= NULL && cur->type == SEXPR_LIST);
    printf("(first (rest expr)) is a SEXPR_NUMBER\n");
    SExpr *one = sexpr_list_first(cur);
    assert(one != NULL && one->type == SEXPR_NUMBER);
    assert(one->value.number == 1);
    printf("done: (first (rest expr)) is a SEXPR_NUMBER\n");
    printf("done: rest is an SEXPR_LIST\n");
    
    // Move to the 2
    cur = sexpr_list_rest(cur);
    assert(cur!= NULL && cur->type == SEXPR_LIST);
    SExpr *two = sexpr_list_first(cur);
    assert(two != NULL && two->type == SEXPR_NUMBER);
    assert(two->value.number == 2);
    
    // Check end of list
    assert(sexpr_list_rest(cur) == NULL);
    printf("list done\n"); 
    free_sexpr(expr);
    printf("test_read_list passed!\n");
}

//void test_read_all() {
//    printf("Testing nested list!\n");
//    const char *input = "(+ 1 2) (- 3 4)";
//    size_t count = 0;
//    SExpr **exprs = read_all(input, &count);
//    assert(count == 2);
//
//    // First expression: (+ 1 2)
//    SExpr *expr = exprs[0];
//    assert(expr != NULL);
//    assert(expr->type == SEXPR_LIST);
//
//    SExpr *cur = expr->value.list;
//    assert(cur != NULL && cur->type == SEXPR_SYMBOL);
//    assert(strcmp(cur->value.symbol, "+") == 0);
//
//    cur = cur->next;
//    assert(cur != NULL && cur->type == SEXPR_NUMBER);
//    assert(cur->value.number == 1);
//
//    cur = cur->next;
//    assert(cur != NULL && cur->type == SEXPR_NUMBER);
//    assert(cur->value.number == 2);
//
//    // Second expression: (- 3 4)
//    expr = exprs[1];
//    assert(expr != NULL);
//    assert(expr->type == SEXPR_LIST);
//
//    cur = expr->value.list;
//    assert(cur != NULL && cur->type == SEXPR_SYMBOL);
//    assert(strcmp(cur->value.symbol, "-") == 0);
//
//    cur = cur->next;
//    assert(cur != NULL && cur->type == SEXPR_NUMBER);
//    assert(cur->value.number == 3);
//
//    cur = cur->next;
//    assert(cur != NULL && cur->type == SEXPR_NUMBER);
//    assert(cur->value.number == 4);
//
//    // Clean up
//    for (size_t i = 0; i < count; i++) {
//        free_sexpr(exprs[i]);
//    }
//    free(exprs);
//    printf("test_read_all passed!\n");
//}
//
//void test_read_nested_list() {
//    printf("Testing nested list!\n");
//    int pos = 0;
//    const char *input = "(+ 1 (- 3 4) (* 2 3))";
//    SExpr *expr = read_list(input, &pos);
//    // Test outer list
//    assert(expr != NULL);
//    assert(expr->type == SEXPR_SYMBOL);
//    assert(strcmp(expr->value.symbol, "+") == 0);
//
//    SExpr *cur = expr->next;
//    assert(cur != NULL && cur->type == SEXPR_NUMBER);
//    assert(cur->value.number == 1);
//    
//    // Test nested list '(- 3 4)'
//    cur = cur->next;
//    assert(cur != NULL && cur->type == SEXPR_LIST);
//    SExpr *nested = cur->value.list;
//    assert(nested != NULL && nested->type == SEXPR_SYMBOL);
//    assert(strcmp(nested->value.symbol, "-") == 0);
//    nested = nested->next;
//    assert(nested != NULL && nested->type == SEXPR_NUMBER);
//    assert(nested->value.number == 3);
//    nested = nested->next;
//    assert(nested != NULL && nested->type == SEXPR_NUMBER);
//    assert(nested->value.number == 4);
//    assert(nested->next == NULL);
//    
//    // Test nested list '(* 2 3)'
//    cur = cur->next;
//    assert(cur != NULL && cur->type == SEXPR_LIST);
//    nested = cur->value.list;
//    assert(nested != NULL && nested->type == SEXPR_SYMBOL);
//    assert(strcmp(nested->value.symbol, "*") == 0);
//    nested = nested->next;
//    assert(nested != NULL && nested->type == SEXPR_NUMBER);
//    assert(nested->value.number == 2);
//    nested = nested->next;
//    assert(nested != NULL && nested->type == SEXPR_NUMBER);
//    assert(nested->value.number == 3);
//    assert(nested->next == NULL);
//    
//    assert(cur->next == NULL);
//    
//    free_sexpr(expr);
//    printf("test_read_nested_list passed!\n");
//}


int main() {
    test_read_number();
    test_read_symbol();
    test_read_string();
    test_read_list();
    //test_read_all();
    //test_read_nested_list();

    printf("All tests passed!\n");
    return 0;
}
