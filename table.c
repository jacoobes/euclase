#include "./table.h"
#include "./memory.h"





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

static void adjust_capacity (HashTable* tbl, int capacity) {
    TableEntry* new_entries = ALLOCATE(TableEntry, capacity) ;
    for(int i = 0; i < capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = lobj_num(0);
    }

    for(int i = 0; i < capacity; i++) {
        TableEntry* old_entry = &tbl->entries[i];
        if (old_entry->key == NULL) {
            continue;
        }
        TableEntry* dest = find_entry(new_entries, capacity, old_entry->key );
        dest->key = old_entry->key;
        dest->value = old_entry->value;
    }
    tbl->entries = new_entries;
    tbl->capacity = capacity;
}

// linear probing
// fnv-1a algo
int table_set(HashTable* tbl, vm_string* k, lobj v) {
    if(tbl->count + 1 > tbl->capacity * TABLE_MAX_LOAD) {
        int new_cap = MAX(11, tbl->capacity*2);
        adjust_capacity(tbl, new_cap);
        //realloc ruins our bucket locations. we need to resize and put items in the right spots.
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
