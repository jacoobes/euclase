#include "../table.h"
#include "assert.h"
#include <stdio.h>

vm_string* make_string(char* chs, unsigned long hash) {
    vm_string* str = (vm_string*)malloc(sizeof(vm_string));
    str->contents = solid_str_new(chs);
    str->hash = hash;
    return str;
}

void test_hashtable() {
    HashTable table;
    table_create(&table);

    // Test: Insert and retrieve a value
    vm_string* key1 = make_string("key1", 12345);
    lobj value1 = lobj_num(42);
    printf("testing table_set\n");
    assert(table_set(&table, key1, value1) == 1);

    lobj retrieved_value;
    printf("testing table_get\n");
    assert(table_get(&table, key1, &retrieved_value) == 1);
    assert(AS_NUM(retrieved_value) == 42);
    printf("testing table_get done\n");

    // Test: Update an existing key
    lobj value2 = lobj_num(84);
    printf("testing table_update\n");
    assert(table_set(&table, key1, value2) == 0);
    assert(table_get(&table, key1, &retrieved_value) == 1);
    assert(AS_NUM(retrieved_value) == 84);
    printf("testing table_update done\n");

    // Test: Delete a key
    printf("testing table_delete\n");
    assert(table_delete(&table, key1) == 1);
    assert(table_get(&table, key1, &retrieved_value) == 0);
    printf("testing table_delete done\n");

    // Test: Handle tombstones
    printf("testing table_tombstone\n");
    assert(table_set(&table, key1, value1) == 1); // Reuse tombstone slot
    assert(table_get(&table, key1, &retrieved_value) == 1);
    assert(AS_NUM(retrieved_value) == 42);
    printf("testing table_tombstone done\n");

    // Test: Handle collisions
    vm_string* key2 = make_string("key2", 12345); // Same hash as key1
    printf("testing table_collision\n");
    lobj value3 = lobj_num(99);
    assert(table_set(&table, key2, value3) == true);
    assert(table_get(&table, key2, &retrieved_value) == true);
    assert(AS_NUM(retrieved_value) == 99);
    printf("testing table_collision done\n");


    // Cleanup
    table_free(&table);
    free(key1);
    free(key2);

    printf("All tests passed!\n");
}


int main() {
    test_hashtable();
}
