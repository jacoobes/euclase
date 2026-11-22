#include "solid_str.h"
#include <stdio.h>

uint32_t hash_str(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

solid_str solid_str_new(const char* startbuf) {
    darray_char s = darray_new();
    if(startbuf == NULL) {
        unsigned long ha = hash_str(startbuf, 0);
        return (solid_str){.length=0,  .vec=s, .hash=ha };
    } else {
        int len = strlen(startbuf);
        unsigned long ha = hash_str(startbuf, len);
        darray_append_items_nullterminate(s, startbuf, len);
        return (solid_str){ .length=len, .vec=s, .hash=ha };
    }
}

void solid_str_free(solid_str* s) {
    darray_free(s->vec);    
}


void solid_str_appends(solid_str* s, char* cs) {
    darray_append_items_nullterminate(s->vec, cs, strlen(cs));
    s->length+=strlen(cs);
    s->hash=hash_str(s->vec.item, s->length);
}

char solid_str_at(solid_str* s, int idx) {
    if(idx >= 0 && idx < s->length) {
        char el = darray_item(s->vec, idx);
        return el;
    }
    return -1;
}

int solid_strcmp(const solid_str* s1, const solid_str* s2) {
    return strcmp(SOLID_STR_TO_C(*s1), SOLID_STR_TO_C(*s2));
}

solid_str solid_str_subs(solid_str* s1, int start, int end) {
    int slen = s1->length;
    int s_end = end;
    if(s_end == -1) {
        s_end = slen;
    }
    // if start is less than length of str and 
    // end less than length of str, it is valid in range
    //  start 2, end -1
    //  [ h e l l o \o ]
    
    if ((start >= 0 && start <= slen) && (s_end >= 0 && s_end <= slen)) {
        darray_char new_str = darray_new();
        darray_append_items_nullterminate(new_str, s1->vec.item+start, s_end-start);
        return (solid_str){ .vec=new_str, 
                            .length=s_end-start };
    }
    printf("Index out of bounds: [%d, %d)\n", start, s_end);
    exit(1);
    
}



char* solid_str_to_c(solid_str* s) {
    return s->vec.item;
}
