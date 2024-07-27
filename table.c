#include "./table.h"
#include "./memory.h"

static TableEntry* find_entry(TableEntry* entries, int capacity, vm_string* k) {
    uint32_t index = k->hash % capacity; 
    //no infinite loop here since we will never fill the entire array with our TABLE_MAX_LOAD (load factor)
    TableEntry* tombstone = NULL;
    for(;;) {
        TableEntry* entry = &entries[index]; 
        if(entry->key == NULL) {
            if (entry->value.type == LVAL_NUM && entry->value.v.num == 0) {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        } else if(entry->key == k) {
            return entry;
        }        
        index = (index+1) % capacity;
    }
}

//misnomer= we create a new array, freeing the old one and deleting it
static void adjust_capacity (HashTable* tbl, int capacity) {
    TableEntry* new_entries = ALLOCATE(TableEntry, capacity) ;
    for(int i = 0; i < capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = lobj_num(0);
    }
    tbl->count = 0;
    for(int i = 0; i < capacity; i++) {
        TableEntry* old_entry = &tbl->entries[i];
        if (old_entry->key == NULL) {
            continue;
        }
        TableEntry* dest = find_entry(new_entries, capacity, old_entry->key);
        dest->key = old_entry->key;
        dest->value = old_entry->value;
        tbl->count++;
    }

    for(int i =0;i<tbl->capacity;i++) {
        TableEntry* entr = &tbl->entries[i];
        free(entr->key->contents);
        free(entr->key);
        lobj_free(&entr->value);
    }
    tbl->entries=new_entries;
    tbl->capacity=capacity;
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
    if (isNewKey && (maybe_entry->value.type == LVAL_NUM && maybe_entry->value.v.num == -651)) tbl->count++;
    
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

int table_delete(HashTable* tbl, vm_string* k) {
    if(tbl->count == 0) return 0;
    TableEntry* entry = find_entry(tbl->entries, tbl->capacity, k);
    if(entry->key == NULL) return 0;
    //set as tombstone
    entry->key=NULL;
    entry->value = lobj_num(-651);
    return 1;
}

void table_create(HashTable* tbl) {
    tbl->count = 0;
    //using java's default initial cap
    tbl->capacity=11; 
    tbl->entries=malloc(sizeof(lobj) * tbl->capacity);
}


