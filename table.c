#include "./table.h"
#include <stdio.h>
#include <stdlib.h>
#include "memory.h"

static TableEntry* find_entry(TableEntry* entries, int capacity, vm_string* k) {
    uint32_t index = k->contents.hash % capacity; 
    //no infinite loop here since we will never fill the entire array with our TABLE_MAX_LOAD (load factor)
    TableEntry* tombstone = NULL;
    for(;;) {
        TableEntry* entry = &entries[index]; 
        if(entry->key == NULL) {
            if (entry->value.type == LVAL_NUM && AS_NUM(entry->value) == TABLE_TOMBSTONE) {
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
    TableEntry* new_entries = calloc(sizeof(TableEntry), capacity);
    for(int i = 0; i < capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = lobj_num(TABLE_TOMBSTONE);
    }
    tbl->count = 0;
    if (tbl->entries) {
        for(int i = 0; i < capacity; i++) {
            const TableEntry* old_entry = &tbl->entries[i];
            if (old_entry->key == NULL) {
                continue;
            }
            TableEntry* dest = find_entry(new_entries, capacity, old_entry->key);
            dest->key = old_entry->key;
            dest->value = old_entry->value;
            tbl->count++;
#if DEBUG
            //printf("old entry moved to new entry in hash table\n");
#endif

        }
        free(tbl->entries);
    }
    tbl->entries=new_entries;
    tbl->capacity=capacity;
}

// linear probing
int table_set(HashTable* tbl, vm_string* k, const lobj v) {
    if(tbl->count + 1 > tbl->capacity * TABLE_MAX_LOAD) {
        int new_cap = MAX(11, tbl->capacity*2);
#if DEBUG
        //printf("Adjusting capacity, new cap = %d\n", new_cap);
#endif

        //reallocate ruins our bucket locations. we need to resize and put items in the right spots.
        adjust_capacity(tbl, new_cap);
    }

    TableEntry* maybe_entry = find_entry(tbl->entries, tbl->capacity, k);
    const int isNewKey = maybe_entry->key == NULL;
    //increase table count
    if (isNewKey && (IS_NUM(maybe_entry->value) && AS_NUM(maybe_entry->value) == TABLE_TOMBSTONE)) tbl->count++;
    
    maybe_entry->key=k;
    maybe_entry->value=v;
    return isNewKey;
}

int table_get(const HashTable* tbl, vm_string* k, lobj* v) {

    if (tbl->count == 0) return false;
    TableEntry* maybe_entry = find_entry(tbl->entries, tbl->capacity, k); 
    if(maybe_entry->key == NULL) {
        return false;
    }
    *v = maybe_entry->value;
    return 1;
}

int table_delete(const HashTable* tbl, vm_string* k) {
    if(tbl->count == 0) 
        return false;
    TableEntry* entry = find_entry(tbl->entries, tbl->capacity, k);
    if(entry->key == NULL) 
        return false;
    //set as tombstone
    entry->key=NULL;
    entry->value = lobj_num(TABLE_TOMBSTONE);
    return true;
}

void table_create(HashTable* tbl) {
    tbl->count = 0;
    tbl->capacity=0; 
    tbl->entries=NULL;
}

vm_string* table_find_string(HashTable* table, const solid_str* ss) {
  if (table->count == 0) return NULL;

  uint32_t index = ss->hash % table->capacity;
  for (;;) {
    TableEntry* entry = &table->entries[index];
    if (entry->key == NULL) {
      if(AS_NUM(entry->value) == TABLE_TOMBSTONE) { 
            //printf("tombstone");
            return NULL;
      }
      //printf("null key\n");
    } else if ((solid_strcmp(ss, &entry->key->contents) == 0) && entry->key->contents.hash == ss->hash) {
      // We found it.
      return entry->key;
    } else {
        printf("%s\n", SOLID_STR_TO_C(entry->key->contents));
        //printf("?!");
    }
    index = (index + 1) % table->capacity;
  }
}

void table_free(HashTable* table) {
    FREE_ARRAY(TableEntry, table->entries, table->count);
    table_create(table);
}

void table_print(HashTable* table) {
    printf("HashTable (capacity: %d, count: %d):\n", table->capacity, table->count);
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        if (entry->key != NULL) { // Assuming NULL key means an empty slot
            printf("  [%d]: key = %s, value =",
            i, SOLID_STR_TO_C(entry->key->contents));
            lobj_println(&entry->value);
        } else {
            printf("  [%d]: <empty>\n", i);
        }
    }
}
void mark_table(HashTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        mark_object((vm_object*)entry->key);
        mark_value(entry->value);
  }
}
