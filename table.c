#include "./table.h"

static TableEntry* find_entry(TableEntry* entries, int capacity, vm_string* k) {
    uint32_t index = k->hash % capacity; 
    //no infinite loop here since we will never fill the entire array with our TABLE_MAX_LOAD (load factor)
    for(;;) {
        TableEntry* entry = &entries[index]; 
        if(entry->key == NULL || entry->key->hash == k->hash) {
            return entry;
        }
        index = (index+1) % capacity;
    }
}

// linear probing
// fnv-1a algo
int table_set(HashTable* tbl, vm_string* k, lobj v) {
    if(tbl->count + 1 > tbl->capacity * TABLE_MAX_LOAD) {
        tbl->capacity *= 2;
        //realloc ruins our bucket locations. we need to resize and put items in the right spots.
        tbl->entries = realloc(tbl->entries, sizeof(lobj)*tbl->capacity);
    }
    TableEntry* maybe_entry = find_entry(tbl->entries, tbl->capacity, k);
    int isNewKey =maybe_entry->key == NULL;
    if (isNewKey) tbl->count++;
    
    maybe_entry->key=k;
    maybe_entry->value=v;
    return isNewKey;
}

int table_get(HashTable* tbl, vm_string* k, lobj* v) {
    if (tbl->count == 0) return 0;
    TableEntry* maybe_entry = find_entry(tbl->entries, tbl->capacity, k); 
    *v = maybe_entry->value;
    return 1;
}

void table_create(HashTable* tbl) {
    tbl->count = 0;
    //using java's default initial cap
    tbl->capacity=11; 
    tbl->entries=malloc(sizeof(lobj) * tbl->capacity);
}
