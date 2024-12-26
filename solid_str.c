#include "solid_str.h"
#include "memory.h"
#include <stdio.h>


solid_str solid_str_new(char* startbuf) {
    darray_char s = darray_new();
    if(startbuf == NULL) {
        return (solid_str){.length=0,  .vec=s };
    } else {
        darray_append_items_nullterminate(s, startbuf, strlen(startbuf));
        return (solid_str){ .length=strlen(startbuf), .vec=s };
    }
}

void solid_str_free(solid_str* s) {
    darray_free(s->vec);    
}


void solid_str_appends(solid_str* s, char* cs) {
    darray_append_items_nullterminate(s->vec, cs, strlen(cs));
    s->length+=strlen(cs);
}

char solid_str_at(solid_str* s, int idx) {
    if(idx >= 0 && idx < s->length) {
        char el = darray_item(s->vec, idx);
        return el;
    }
    return -1;
}

int solid_strcmp(solid_str* s1, solid_str* s2) {
    return strcmp(SOLID_STR_TO_C(*s1), SOLID_STR_TO_C(*s2));
}

/*
 * Copy string src to buffer dst of size dsize.  At most dsize-1
 * chars will be copied.  Always NUL terminates (unless dsize == 0).
 * Returns strlen(src); if retval >= dsize, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';		/* NUL-terminate dst */
		while (*src++)
			;
	}

	return(src - osrc - 1);	/* count does not include NUL */
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
