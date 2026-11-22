#include "../table.h"
#include "assert.h"
#include <stdio.h>

vm_string* make_string(char* chs) {
    vm_string* str = (vm_string*)malloc(sizeof(vm_string));
    str->contents = solid_str_new(chs);
    
    return str;
}

void test_hashtable() {
    HashTable table;
    table_create(&table);

    // Test: Insert and retrieve a value
    vm_string* key1 = make_string("key1");
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
    vm_string* key2 = make_string("key2"); // Same hash as key1
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


// Growing the table several times must keep every entry (issue #2: the rehash
// loop used the new capacity as its bound and read past the old array).
void test_hashtable_growth() {
    enum { N = 200 };
    HashTable table;
    table_create(&table);
    vm_string* keys[N];
    char buf[16];

    for (int i = 0; i < N; i++) {
        snprintf(buf, sizeof buf, "key%d", i);
        keys[i] = make_string(buf);
        assert(table_set(&table, keys[i], lobj_num(i)) == 1);
        assert(table.count == i + 1);
        // the load factor must hold after every insert
        assert(table.count <= table.capacity * TABLE_MAX_LOAD);
        // everything inserted so far must survive the resize
        for (int j = 0; j <= i; j++) {
            lobj v;
            assert(table_get(&table, keys[j], &v) == 1);
            assert(IS_NUM(v) && AS_NUM(v) == j);
        }
    }
    // 11 -> 22 -> 44 -> 88 -> 176 -> 352
    assert(table.capacity == 352);

    // updating existing keys must not add entries
    for (int i = 0; i < N; i++) {
        assert(table_set(&table, keys[i], lobj_num(i * 2)) == 0);
    }
    assert(table.count == N);

    // lookup by characters still finds the same key object after rehashing
    for (int i = 0; i < N; i++) {
        lobj v;
        assert(table_get(&table, keys[i], &v) == 1);
        assert(AS_NUM(v) == i * 2);
        snprintf(buf, sizeof buf, "key%d", i);
        assert(table_find_string(&table, buf, keys[i]->contents.hash) == keys[i]);
    }
    assert(table_find_string(&table, "missing", hash_str("missing", 7)) == NULL);

    table_free(&table);
    for (int i = 0; i < N; i++) {
        solid_str_free(&keys[i]->contents);
        free(keys[i]);
    }
    printf("test_hashtable_growth passed!\n");
}


// Deleting a key must not hide other keys that collided with it and were
// stored further along the probe sequence.
void test_hashtable_delete_keeps_colliding_keys() {
    // 7 entries is the most the table holds at capacity 11 without resizing
    // on the next table_set; "k0" and "k14" land in the same bucket
    enum { N = 7 };
    const char* names[N] = { "k0", "k14", "k1", "k2", "k3", "k4", "k5" };
    HashTable table;
    table_create(&table);
    vm_string* keys[N];
    char buf[16];
    for (int i = 0; i < N; i++) {
        keys[i] = make_string((char*)names[i]);
        assert(table_set(&table, keys[i], lobj_num(i)) == 1);
    }
    assert(table.capacity == 11);
    assert(keys[0]->contents.hash % 11 == keys[1]->contents.hash % 11);
    // delete each key in turn and check that every other key is still reachable
    for (int d = 0; d < N; d++) {
        lobj v;
        assert(table_delete(&table, keys[d]) == 1);
        assert(table_get(&table, keys[d], &v) == 0);
        assert(table_delete(&table, keys[d]) == 0);
        for (int j = 0; j < N; j++) {
            if (j == d) continue;
            assert(table_get(&table, keys[j], &v) == 1);
            assert(AS_NUM(v) == j);
            // setting a key that is still present must update it, not add a duplicate
            assert(table_set(&table, keys[j], lobj_num(j)) == 0);
            assert(table_find_string(&table, names[j], keys[j]->contents.hash) == keys[j]);
        }
        // put it back (reuses the tombstone) for the next round
        assert(table_set(&table, keys[d], lobj_num(d)) == 1);
        assert(table_get(&table, keys[d], &v) == 1 && AS_NUM(v) == d);
    }
    assert(table.capacity == 11);

    // tombstones are dropped on resize and live keys survive it
    assert(table_delete(&table, keys[0]) == 1);
    vm_string* extra[4];
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof buf, "extra%d", i);
        extra[i] = make_string(buf);
        assert(table_set(&table, extra[i], lobj_num(100 + i)) == 1);
    }
    assert(table.capacity == 22);
    assert(table.count == N - 1 + 4);
    for (int j = 1; j < N; j++) {
        lobj v;
        assert(table_get(&table, keys[j], &v) == 1 && AS_NUM(v) == j);
    }
    lobj v;
    assert(table_get(&table, keys[0], &v) == 0);

    table_free(&table);
    for (int i = 0; i < N; i++) { solid_str_free(&keys[i]->contents); free(keys[i]); }
    for (int i = 0; i < 4; i++) { solid_str_free(&extra[i]->contents); free(extra[i]); }
    printf("test_hashtable_delete_keeps_colliding_keys passed!\n");
}


int main() {
    test_hashtable();
    test_hashtable_growth();
    test_hashtable_delete_keeps_colliding_keys();
}
