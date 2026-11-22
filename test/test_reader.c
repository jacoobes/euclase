#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
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



    int pos3 = 0;
    SExpr* expr3 = read_number("456.56", -1, &pos3);
    assert(expr3 != NULL);
    assert(expr3->type == SEXPR_NUMBER);
    assert(expr3->value.number == -456.56);
    free_sexpr(expr3);
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


void test_read_atom() {
    int pos = 0;
    SExpr *expr = read_atom("-42", &pos);
    assert(expr != NULL);
    assert(expr->type == SEXPR_NUMBER);
    assert(expr->value.number == -42);
    free_sexpr(expr);

    int pos2 = 0;
    SExpr *expr2 = read_atom("abc", &pos2);
    assert(expr2 != NULL);
    assert(expr2->type == SEXPR_SYMBOL);
    assert(strcmp(expr2->value.symbol, "abc") == 0);
    free_sexpr(expr2);

    int pos3 = 0;
    SExpr *expr3 = read_atom("\"abcdefg\"", &pos3);
    assert(expr3 != NULL);
    assert(expr3->type == SEXPR_STRING);
    assert(strcmp(expr3->value.string, "abcdefg") == 0);
    free_sexpr(expr3);

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
    const char* input3 = "( f 10 )(fne 12 )()(())";
    printf("test_sexpr_list_adj\n");
    size_t count2;
    SExpr** b = read_all(input3, &count2);
    assert(b != NULL);
    assert(count2 == 4);
    
    SExpr* form0 = b[0];
    assert(sexpr_list_first(form0)->type == SEXPR_SYMBOL && strcmp(sexpr_list_first(form0)->value.symbol, "f") == 0);
    SExpr* rest1 = sexpr_list_rest(form0);
    assert(rest1 != NULL);
    SExpr* elem2 = sexpr_list_first(rest1);
    assert(elem2 != NULL);
    assert(elem2->type == SEXPR_NUMBER);
    assert(elem2->value.number == 10);

    SExpr* rest2 = sexpr_list_rest(rest1);
    assert(rest2 == NULL);

    SExpr* form1 = b[1];
    assert(sexpr_list_first(form1)->type == SEXPR_SYMBOL && strcmp(sexpr_list_first(form1)->value.symbol, "fne") == 0);
    SExpr* rest3 = sexpr_list_rest(form1);
    assert(rest3 != NULL);
    SExpr* elem4 = sexpr_list_first(rest3);
    assert(elem4 != NULL);
    assert(elem4->type == SEXPR_NUMBER);
    assert(elem4->value.number == 12);
    SExpr* rest4 = sexpr_list_rest(rest3);
    assert(rest4 == NULL);

    SExpr* form2 = b[2];
    assert(form2->type == SEXPR_LIST);
    assert(sexpr_list_first(form2) == NULL);
    assert(sexpr_list_rest(form2) == NULL);

    SExpr* form3 = b[3];
    assert(form3->type == SEXPR_LIST);
    SExpr* form3_elem = sexpr_list_first(form3);
    assert(form3_elem != NULL);
    assert(form3_elem->type == SEXPR_LIST);
    assert(sexpr_list_first(form3_elem) == NULL);
    assert(sexpr_list_rest(form3_elem) == NULL);
    SExpr* form3_rest = sexpr_list_rest(form3);
    assert(form3_rest == NULL);
    printf("done: adjacent lists2 are adjacent\n");
}


// An unterminated list must be rejected with exit(1) instead of reading past
// the end of the input (issue #5). read_list exits, so run it in a child.
static void expect_unterminated(const char* src) {
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        // exact-size heap copy so sanitizers catch any read past the NUL
        char* input = malloc(strlen(src) + 1);
        strcpy(input, src);
        int pos = 0;
        read_list(input, &pos);
        _exit(0); // not reached once the reader rejects the input
    }
    int status = 0;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

void test_read_unterminated_list() {
    expect_unterminated("((");
    expect_unterminated("(");
    expect_unterminated("(+ 1 2");
    expect_unterminated("(+ 1 (- 2 1)");
    printf("test_read_unterminated_list passed!\n");
}

// Input that ends in whitespace or a comment must parse cleanly without
// reading past the end of the buffer (issue #4).
static size_t count_exprs(const char* src) {
    // exact-size heap copy so sanitizers catch any read past the NUL
    char* input = malloc(strlen(src) + 1);
    strcpy(input, src);
    size_t count = 99;
    read_all(input, &count);
    free(input);
    return count;
}

void test_read_all_trailing_whitespace() {
    assert(count_exprs("(+ 1 2)\n") == 1);
    assert(count_exprs("(+ 1 2) ") == 1);
    assert(count_exprs("(+ 1 2)\n\n(+ 3 4)\n") == 2);
    assert(count_exprs("(+ 1 2) ; comment\n") == 1);
    assert(count_exprs("(+ 1 2) ; comment") == 1);
    assert(count_exprs("42\n") == 1);
    assert(count_exprs("\n") == 0);
    assert(count_exprs("  \t\n ") == 0);
    assert(count_exprs("") == 0);
    printf("test_read_all_trailing_whitespace passed!\n");
}

// Symbols are capped at 255 characters (the 256-byte buffer minus the NUL).
// Longer input must be rejected with exit(1) instead of writing past the buffer
// (issue #6).
static char* make_symbol(int n) {
    char* s = malloc(n + 1);
    memset(s, 'a', n);
    s[n] = '\0';
    return s;
}

static int read_symbol_in_child(int n) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        char* input = make_symbol(n);
        int pos = 0;
        read_symbol(input, &pos);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return status;
}

void test_read_symbol_length_limit() {
    // the longest allowed symbol reads back intact and NUL-terminated
    char* input = make_symbol(255);
    int pos = 0;
    SExpr* e = read_symbol(input, &pos);
    assert(e->type == SEXPR_SYMBOL);
    assert(strlen(e->value.symbol) == 255);
    assert(strcmp(e->value.symbol, input) == 0);
    assert(pos == 255);
    free(input);

    int st = read_symbol_in_child(255);
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    // one character too many, and far too many, are both rejected
    st = read_symbol_in_child(256);
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 1);
    st = read_symbol_in_child(300);
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 1);
    printf("test_read_symbol_length_limit passed!\n");
}

int main() {
    test_skip_whitespace();
    test_read_number();
    test_read_symbol();
    test_read_string();
    test_read_atom();
    test_read_list();
    test_read_empty_list();
    test_sexpr_list_len();
    test_sexpr_list_spacing();
    test_read_nested_list();
    test_sexpr_list_adj();
    test_sexpr_list_read_all();
    test_sexpr_list_read_all2();
    test_read_unterminated_list();
    test_read_all_trailing_whitespace();
    test_read_symbol_length_limit();

    // todo handle lists that have NO SPACE between each other
    //test_read_all();

    printf("All tests passed!\n");
    return 0;
}
