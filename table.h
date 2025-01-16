#ifndef table_h
#define table_h

#include "lobj.h"
#define MAX(x, y)           (((x) > (y)) ?  (x) : (y))


// We don’t grow when the capacity is completely full.
// Instead, we grow the array before then, when the array becomes at least 75% full.
#define TABLE_MAX_LOAD 0.75
#define TABLE_TOMBSTONE (-651)

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
int table_get(HashTable* tbl, vm_string*, lobj*);
int table_delete(HashTable* tbl, vm_string*);
void table_free(HashTable* table);
vm_string* table_find_string(HashTable* table, const char* chars, int length, uint32_t hash);



#endif
