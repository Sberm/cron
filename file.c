#include <stdio.h>

#include "util.h"
#include "file.h"
#include "vec.h"

#define STEP 256

void *read_line_v(FILE *f)
{
    int err = 0;
    char *vbuf = vec__new(sizeof(char));
    size_t idx = 0;
    size_t bytes;

    vec__reserve(vbuf, STEP);
    while ((bytes = fread(__vec__at(vbuf, idx), vec__mem_size(vbuf), min(vec__cap(vbuf) - idx, STEP), f))) {
        size_t pre_idx = idx;
        char *vbuf_tmp = NULL;

        idx += bytes;
        vec__len_inc(vbuf, bytes / vec__mem_size(vbuf));

        /* if the vector is full, there's more data (or data size = vec size) */
        if (vec__cap(vbuf) == idx)
            vec__alloc(vbuf, STEP);

        /* after vec__alloc for possible realloc */
        vbuf_tmp = __vec__at(vbuf, 0);

        /* check if there's a \n */
        for (size_t i = pre_idx; i < idx; i++) {
            if (vbuf_tmp[i] == '\n') {
                vec__resize(vbuf, i / vec__mem_size(vbuf));
                break;
            }
        }
    }

    // insert null term
    if (vec__cap(vbuf) >= vec__len(vbuf) + 1)
        ((char *)__vec__at(vbuf, 0))[vec__len(vbuf)] = '\0';
    else
        *((char *)__vec__at(vbuf, vec__len(vbuf))) = '\0';

    pr_debug("%s\n", (char *)__vec__at(vbuf, 0));

    return vbuf;
}

