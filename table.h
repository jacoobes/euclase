#ifndef table_h
#define table_h

#include "solid_str.h"
#include "vm.h"

#define MAX(x, y)           (((x) > (y)) ?  (x) : (y))


// We don’t grow when the capacity is completely full.
// Instead, we grow the array before then, when the array becomes at least 75% full.
#define TABLE_MAX_LOAD 0.75
#define TABLE_TOMBSTONE 0

typedef struct TableEntry {
    vm_string* key;
    lobj value;
} TableEntry;

typedef struct HashTable {
    int capacity; // max num to be held
    int count; // current count
    TableEntry* entries; 
} HashTable;


void table_create(HashTable* tbl);
int table_set(HashTable* tbl, vm_string*, lobj);
int table_get(const HashTable* tbl, vm_string*, lobj*);
int table_delete(const HashTable* tbl, vm_string*);
void table_free(HashTable* table);
vm_string* table_find_string(HashTable* table, const char* ss, uint32_t hash);
void table_print(HashTable* table);

void table_mark_white(HashTable* table);
void mark_table(VM* vm, HashTable* table);

#endif
