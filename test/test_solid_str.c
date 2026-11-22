#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../solid_str.h"
// Assuming these are defined in your header file

void test_solid_str_new() {
    printf("Testing solid_str_new...\n");
    
    // Test with empty string
    solid_str s2 = solid_str_new("");
    assert(solid_strlen(&s2) == 0);
    solid_str_free(&s2);
    
    // Test with normal string
    solid_str s3 = solid_str_new("hello");
    assert(solid_strlen(&s3)== 5);
    assert(strcmp(SOLID_STR_TO_C(s3), "hello") == 0);
    solid_str_free(&s3);
    
    printf("solid_str_new tests passed!\n");
}


void test_solid_str_appends() {
    printf("Testing solid_str_appends...\n");
    
    solid_str s = solid_str_new("hello");
    
    // Test appending empty string
    solid_str_appends(&s, "");
    assert(solid_strlen(&s) == 5);
    assert(strcmp(SOLID_STR_TO_C(s), "hello") == 0);
    // Test appending non-empty string
    solid_str_appends(&s, " world");
    assert(solid_strlen(&s)== 11);
    printf("%s", SOLID_STR_TO_C(s));
    assert(strcmp(SOLID_STR_TO_C(s), "hello world") == 0);
    
    solid_str_free(&s);
    printf("solid_str_appends tests passed!\n");
}

void test_solid_str_at() {
    printf("Testing solid_str_at...\n");
    
    // Test with normal string
    solid_str s = solid_str_new("Hello World!");
    // Test first character
    assert(solid_str_at(&s, 0) == 'H');
    
    // Test middle character
    assert(solid_str_at(&s, 6) == 'W');
    
    // Test last character
    assert(solid_str_at(&s, 11) == '!');
    
    // Test space character
    assert(solid_str_at(&s, 5) == ' ');
    
    // Create a new string with special characters
    solid_str s2 = solid_str_new("123!@#");
    
    // Test numeric character
    assert(solid_str_at(&s2, 0) == '1');
    
    // Test special character
    assert(solid_str_at(&s2, 3) == '!');
    assert(solid_str_at(&s2, 6) == -1);
    solid_str_free(&s);
    solid_str_free(&s2);
    printf("solid_str_at tests passed!\n");
}

void test_solid_str_eq() {
    printf("Testing solid_str_eq...\n");
    
    // Test equal strings
    solid_str s1 = solid_str_new("hello");
    solid_str s2 = solid_str_new("hello");
    assert(solid_strcmp(&s1, &s2) == 0);  // Should return 0 for equal strings
    
    // Test different strings of same length
    solid_str s3 = solid_str_new("world");
    assert(solid_strcmp(&s1, &s3) < 0);  // 'h' comes before 'w'
    assert(solid_strcmp(&s3, &s1) > 0);  // 'w' comes after 'h'
    
    // Test strings of different lengths
    solid_str s4 = solid_str_new("hello!");
    assert(solid_strcmp(&s1, &s4) < 0);  // "hello" comes before "hello!"
    assert(solid_strcmp(&s4, &s1) > 0);  // "hello!" comes after "hello"
    
    // Test empty strings
    solid_str empty1 = solid_str_new("");
    solid_str empty2 = solid_str_new("");
    assert(solid_strcmp(&empty1, &empty2) == 0);  // Empty strings are equal
    
    // Test empty string with non-empty string
    assert(solid_strcmp(&empty1, &s1) < 0);  // Empty string comes before non-empty
    assert(solid_strcmp(&s1, &empty1) > 0);  // Non-empty string comes after empty
    
    // Test strings with special characters
    solid_str special1 = solid_str_new("hello!");
    solid_str special2 = solid_str_new("hello?");
    assert(solid_strcmp(&special1, &special2) < 0);  // '!' comes before '?'
    
    // Test case sensitivity
    solid_str upper = solid_str_new("HELLO");
    assert(solid_strcmp(&s1, &upper) > 0);  // lowercase comes after uppercase
    
    // Clean up
    solid_str_free(&s1);
    solid_str_free(&s2);
    solid_str_free(&s3);
    solid_str_free(&s4);
    solid_str_free(&empty1);
    solid_str_free(&empty2);
    solid_str_free(&special1);
    solid_str_free(&special2);
    solid_str_free(&upper);
    
    printf("solid_str_eq tests passed!\n");
}


void test_solid_str_subs1( ) {
    printf("Testing solid_str_subs1...\n");
    solid_str s1 = solid_str_new("hello");
    solid_str without_h = solid_str_subs(&s1, 1, -1);

    solid_str expected1 = solid_str_new("ello");
    assert(solid_strcmp(&expected1, &without_h) == 0);
    solid_str_free(&s1);
    solid_str_free(&without_h);
    solid_str_free(&expected1);

    printf("solid_str_subs1 tests passed\n");
}

void test_solid_str_subs2( ) {
    printf("Testing solid_str_subs2...\n");
    solid_str s2 = solid_str_new("hello");
    solid_str e = solid_str_subs(&s2, 1, 2);
    assert(solid_strlen(&e) == 1);
    assert(solid_strlen(&s2) == 5);

    solid_str expected2 = solid_str_new("e");
    assert(solid_strcmp(&expected2, &e) == 0);
    solid_str_free(&s2);
    solid_str_free(&e);
    solid_str_free(&expected2);
    printf("solid_str_subs2 tests passed\n");
}

void test_solid_str_subs3( ) {

    printf("Testing solid_str_subs3...\n");
    solid_str s2 = solid_str_new("hello");
    solid_str e = solid_str_subs(&s2, 1, 1);
    assert(solid_strlen(&e) == 0);

    solid_str expected2 = solid_str_new("");
    assert(solid_strcmp(&expected2, &e) == 0);
    solid_str_free(&s2);
    solid_str_free(&e);
    solid_str_free(&expected2);

    printf("solid_str_subs3 tests passed\n");
}

void test_solid_str_subs4( ) {

    printf("Testing solid_str_subs4...\n");
    solid_str s2 = solid_str_new("");
    solid_str e = solid_str_subs(&s2, 0, 0);
    assert(solid_strlen(&e) == 0);
    assert(solid_strlen(&s2) == 0);

    solid_str expected2 = solid_str_new("");
    assert(solid_strcmp(&expected2, &e) == 0);
    solid_str_free(&s2);
    solid_str_free(&e);
    solid_str_free(&expected2);
    printf("solid_str_subs4 tests passed\n");
}


void test_solid_str_subs5() {
    printf("Testing solid_str_subs5...\n");
    solid_str s2 = solid_str_new("abcdef");
    int len = solid_strlen(&s2);
    assert(solid_strlen(&s2) == 6);
    solid_str e = solid_str_subs(&s2, len, len);
    assert(solid_strlen(&e) == 0);
    solid_str expected2 = solid_str_new("");
    assert(solid_strcmp(&expected2, &e) == 0);
    solid_str_free(&s2);
    solid_str_free(&e);
    solid_str_free(&expected2);
    printf("solid_str_subs5 tests passed\n");
}

void test_solid_str_indexoob() {
    int start = 0;
    int end = 6;
    solid_str s1 = solid_str_new("hello");
    int slen = solid_strlen(&s1);
    int s_end = end;
    if(s_end == -1) {
        s_end = slen;
    }
    
    if ((start >= 0 && start <= slen) && (s_end >= 0 && s_end <= slen)) {
        printf("In range");
        exit(1);     
    } 

    printf("solid_str_oob tests passed!\n");
}

// Misuse of the API must stop the process at the check, not corrupt memory
// (issue #16). Each case runs in a child so the test itself survives.
static int run_in_child(void (*fn)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        fn();
        _exit(0); // not reached when the check fires
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return status;
}

static void new_null(void) { solid_str_new(NULL); }
static void append_self(void) {
    solid_str s = solid_str_new("hello");
    solid_str_appends(&s, SOLID_STR_TO_C(s));
}
static void append_own_suffix(void) {
    solid_str s = solid_str_new("hello");
    solid_str_appends(&s, SOLID_STR_TO_C(s) + 3);
}
static void subs_start_after_end(void) {
    solid_str s = solid_str_new("hello");
    solid_str_subs(&s, 2, 1);
}
static void subs_start_far_after_end(void) {
    solid_str s = solid_str_new("hello");
    solid_str_subs(&s, 3, 1); // used to loop forever
}

void test_solid_str_misuse() {
    int st;
    // asserts abort the process
    st = run_in_child(new_null);
    assert(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
    st = run_in_child(append_self);
    assert(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
    st = run_in_child(append_own_suffix);
    assert(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
    // start > end is reported and exits with status 1
    st = run_in_child(subs_start_after_end);
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 1);
    st = run_in_child(subs_start_far_after_end);
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 1);
    printf("solid_str_misuse tests passed!\n");
}

int main() {
    printf("Running solid_str tests...\n\n");
    
    test_solid_str_new();
    test_solid_str_appends();
    test_solid_str_at();
    test_solid_str_eq();
    test_solid_str_subs1();
    test_solid_str_subs2();
    test_solid_str_subs3();
    test_solid_str_subs4();
    test_solid_str_subs5();
    test_solid_str_indexoob();
    test_solid_str_misuse();
    printf("\nAll tests passed successfully!\n");
    return 0;
}
