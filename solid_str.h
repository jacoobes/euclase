#ifndef SOLID_STRH
#define SOLID_STRH

#include "darray.h"
#include <stdint.h>


typedef struct {
    int length;
    darray_char vec;
    unsigned long hash;
} solid_str;



#define SOLID_STR_TO_C(ss)  \
    ((ss).vec.item)

static inline size_t solid_strlen(solid_str* s) { return s->length; }

solid_str solid_str_new(const char* startbuf);
void solid_str_free(solid_str* s);
void solid_str_appends(solid_str* s, char* cs);
char solid_str_at(solid_str* s, int idx);
int solid_strcmp(const solid_str* s1, const solid_str* s2);
solid_str solid_str_subs(solid_str* s1, int start, int end);

#endif
