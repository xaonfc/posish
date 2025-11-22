/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "memalloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error.h"

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        error_fatal("memory allocation failed");
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        error_fatal("memory allocation failed");
    }
    return new_ptr;
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) {
        error_fatal("memory allocation failed");
    }
    return dup;
}
