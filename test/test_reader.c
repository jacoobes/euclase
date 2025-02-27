#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../reader.h"

void test_skip_whitespace() {
    const char* input4 = " ";
    int count3=0;
    skip_whitespace(input4, &count3);

    assert(count3 == 1);

    const char* input5 = "   (";
    int count4=0;
    char c = skip_whitespace(input5, &count4);
    assert(c == '(');
}

void test_read_number() {
    int pos = 0;
    SExpr *expr = read_number("123", 1, &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_NUMBER);
    assert(expr->value.number == 123);
    free_sexpr(expr);


    int pos2 = 0;
    SExpr* expr2 = read_number("456", -1, &pos2);
    assert(expr2 != NULL);
    assert(expr2->type == SEXPR_NUMBER);
    assert(expr2->value.number == -456.0);
    free_sexpr(expr2);

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
void test_read_empty_list() {
    printf("test read nested list\n");
    int pos = 0;
    const char *input = "()";
    SExpr *expr = read_list(input, &pos);
    // Check the outer list
    assert(expr != NULL);
    assert (expr->type == SEXPR_LIST );

    int pos2 = 0;
    const char *input2 = "((()))";
    SExpr *expr2 = read_list(input2, &pos2);
    // Check the outer list
    assert(expr2 != NULL);
    assert (expr2->type == SEXPR_LIST);
    printf("done test read nested list\n");

}

void test_read_nested_list() {
    printf("test read nested list\n");
    int pos = 0;
    const char *input = "(fn (add ()) )";
    SExpr *expr = read_list(input, &pos);
    // Check the outer list
    assert(expr != NULL);
    assert (expr->type == SEXPR_LIST );

    SExpr *cur = sexpr_list_first(expr);
    assert(cur != NULL && cur->type == SEXPR_SYMBOL);

    assert(strcmp(cur->value.symbol, "fn") == 0);

    SExpr *args = sexpr_list_first(sexpr_list_rest(expr));
    assert(args != NULL && args->type == SEXPR_LIST);

    printf("done: expr is a nested LIST\n");

}

void test_sexpr_list_len() {
    const char *input = "()";
    int pos = 0;
    SExpr *expr = read_list(input, &pos);
    printf("test len of LIST\n");
    assert(expr != NULL);
    assert(expr->type == SEXPR_LIST);
    assert(sexpr_list_len(expr) == 0);
    printf("end test len of LIST\n");

    const char *input2 = "(1 2)";
    int pos2 = 0;
    SExpr *expr2 = read_list(input2, &pos2);
    printf("test len of LIST\n");
    assert(sexpr_list_len(expr2) == 2);
    printf("end test len of LIST\n");
}

void test_sexpr_list_read_all() {
    const char *input2 = "(1 2) (3 sad)";
    size_t count;
    printf("test list_read_all\n");
    SExpr** a = read_all(input2, &count);
    assert(a != NULL);
    assert(count == 2);
}



void test_sexpr_list_spacing() {
    const char *input = "( 1   2    )";
    int pos = 0;
    SExpr *li  = read_list(input, &pos);
    assert(li != NULL);
    assert(li->type == SEXPR_LIST);

    assert(sexpr_list_first(li)->type == SEXPR_NUMBER && sexpr_list_first(li)->value.number == 1);
    SExpr *cur = sexpr_list_rest(li);
    assert(sexpr_list_first(cur)->type == SEXPR_NUMBER && sexpr_list_first(cur)->value.number == 2);

}

void test_sexpr_list_read_all2() {
    const char* input3 = "(fn ( add) 10) (fn (sad ) ea)";
    printf("test list_read_all2\n");
    size_t count2;
    SExpr** b = read_all(input3, &count2);
    assert(b != NULL);
    assert(count2 == 2);
    printf("done: lists2\n");
}

void test_sexpr_list_adj() {
    const char* input3 = "(f 10 )(fne 12 )()(())";
    printf("test_sexpr_list_adj\n");
    size_t count2;
    SExpr** b = read_all(input3, &count2);
    assert(b != NULL);
    assert(count2 == 4);
    printf("done: adjacent lists2 are adjacent\n");
}


int main() {
    test_skip_whitespace();
    test_read_number();
    test_read_symbol();
    test_read_string();
    test_read_list();
    test_read_empty_list();
    test_sexpr_list_len();
    test_sexpr_list_spacing();
    test_read_nested_list();
    test_sexpr_list_adj();
    test_sexpr_list_read_all();
    test_sexpr_list_read_all2();

    // todo handle lists that have NO SPACE between each other
    //test_read_all();

    printf("All tests passed!\n");
    return 0;
}
