/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "memalloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error.h"

/* Safe malloc wrappers */

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

/* Stack Allocator Implementation */

/* Alignment macros */
#define SHELL_ALIGN(n) (((n) + sizeof(long) - 1) & ~(sizeof(long) - 1))
#define MINSIZE 4096 // 4KB blocks

struct stack_block {
    struct stack_block *prev;
    char space[MINSIZE];
};

struct stack_block stackbase;
struct stack_block *stackp = &stackbase;
char *stacknxt = stackbase.space;
size_t stacknleft = MINSIZE;
char *sstrend = stackbase.space + MINSIZE;

static void outofspace(void) {
    fprintf(stderr, "Out of memory in stack allocator\n");
    exit(2);
}

void *mem_stack_alloc(size_t nbytes) {
    char *p;
    size_t aligned;

    aligned = SHELL_ALIGN(nbytes);
    if (aligned > stacknleft) {
        size_t len;
        size_t blocksize;
        struct stack_block *sp;

        blocksize = aligned;
        if (blocksize < MINSIZE)
            blocksize = MINSIZE;
        len = sizeof(struct stack_block) - MINSIZE + blocksize;
        
        sp = malloc(len);
        if (!sp) outofspace();
        
        sp->prev = stackp;
        stacknxt = sp->space;
        stacknleft = blocksize;
        sstrend = stacknxt + blocksize;
        stackp = sp;
    }
    p = stacknxt;
    stacknxt += aligned;
    stacknleft -= aligned;
    // fprintf(stderr, "ALLOC %zu bytes at %p. Left=%zu\n", nbytes, (void*)p, stacknleft);
    return p;
}

char *mem_stack_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = mem_stack_alloc(len);
    memcpy(p, s, len);
    return p;
}

void mem_stack_push_mark(struct stackmark *mark) {
    mark->stackp = stackp;
    mark->stacknxt = stacknxt;
    mark->stacknleft = stacknleft;
}

void mem_stack_pop_mark(struct stackmark *mark) {
    struct stack_block *sp;

    while (stackp != mark->stackp) {
        sp = stackp;
        stackp = sp->prev;
        free(sp);
    }
    stacknxt = mark->stacknxt;
    stacknleft = mark->stacknleft;
    sstrend = stacknxt + stacknleft;
}

/* Finalize a string being built on the stack */
void *mem_stack_grab_str(char *p) {
    size_t len = p - stacknxt;
    return mem_stack_alloc(len);
}

void *mem_stack_realloc_array(void *ptr, size_t old_count, size_t new_count, size_t element_size) {
    void *new_ptr = mem_stack_alloc(new_count * element_size);
    if (ptr && old_count > 0) {
        memcpy(new_ptr, ptr, old_count * element_size);
    }
    return new_ptr;
}
