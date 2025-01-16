#include "./table.h"
#include "./memory.h"
#include "lobj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TableEntry* find_entry(TableEntry* entries, int capacity, vm_string* k) {
    uint32_t index = k->hash % capacity; 
    //no infinite loop here since we will never fill the entire array with our TABLE_MAX_LOAD (load factor)
    TableEntry* tombstone = NULL;
    for(;;) {
        TableEntry* entry = &entries[index]; 
        if(entry->key == NULL) {
            if (entry->value.type == LVAL_NUM && AS_NUM(entry->value)== TABLE_TOMBSTONE) {
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
    TableEntry* new_entries = ALLOCATE(TableEntry, capacity);
    for(int i = 0; i < capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = lobj_num(TABLE_TOMBSTONE);
    }
    tbl->count = 0;
    if (tbl->entries) {
        for(int i = 0; i < capacity; i++) {
            TableEntry* old_entry = &tbl->entries[i];
            if (old_entry->key == NULL) {
                continue;
            }
            TableEntry* dest = find_entry(new_entries, capacity, old_entry->key);
            dest->key = old_entry->key;
            dest->value = old_entry->value;
            tbl->count++;
#if DEBUG
            printf("old entry moved to new entry in hash table\n");
#endif

        }
        free(tbl->entries);
    }
    tbl->entries=new_entries;
    tbl->capacity=capacity;
}

// linear probing
// fnv-1a algo
int table_set(HashTable* tbl, vm_string* k, lobj v) {
    if(tbl->count + 1 > tbl->capacity * TABLE_MAX_LOAD) {
        int new_cap = MAX(11, tbl->capacity*2);
#if DEBUG
        printf("Adjusting capacity, new cap = %d\n", new_cap);
#endif
        adjust_capacity(tbl, new_cap);
        //realloc ruins our bucket locations. we need to resize and put items in the right spots.
    }
    TableEntry* maybe_entry = find_entry(tbl->entries, tbl->capacity, k);
    int isNewKey =maybe_entry->key == NULL;
    //increase table count
    if (isNewKey && (IS_NUM(maybe_entry->value) && AS_NUM(maybe_entry->value) == TABLE_TOMBSTONE)) tbl->count++;
    
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
    entry->value = lobj_num(TABLE_TOMBSTONE);
    return 1;
}

void table_create(HashTable* tbl) {
    tbl->count = 0;
    tbl->capacity=0; 
    tbl->entries=NULL;
}

vm_string* table_find_string(HashTable* table, const char* chars,
                           int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    TableEntry* entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      // todo: if (IS_NIL(entry->value)) return NULL;
    } else if (solid_strlen(&entry->key->contents) == length &&
                            entry->key->hash == hash && 
                            memcmp(SOLID_STR_TO_C(entry->key->contents), chars, length) == 0) {
      // We found it.
      return entry->key;
    }
    index = (index + 1) % table->capacity;
  }
}

void table_free(HashTable* table) {
    FREE_ARRAY(TableEntry, table->entries, table->count);
    table_create(table);
}

