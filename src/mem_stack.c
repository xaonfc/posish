/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mem_stack.h"

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

/* String building functions - Simplified for now */
/* We will implement full string growing later if needed. 
   For now, we provide stubs or basic implementations. */

void *mem_stack_grow_str(void) {
    // TODO: Implement proper string growing
    outofspace(); // Fail if we exceed block size for now
    return NULL;
}

char *mem_stack_grow_to(size_t len) {
    (void)len;
    // TODO
    return NULL;
}

char *mem_stack_make_space(size_t n, char *p) {
    (void)n;
    (void)p;
    // TODO
    return NULL;
}

char *mem_stack_put_s(const char *s, char *p) {
    (void)s;
    (void)p;
    // TODO
    return NULL;
}

void *mem_stack_grab_str(char *p) {
    size_t len = p - stacknxt;
    return mem_stack_alloc(len); // This effectively finalizes it
}

void *mem_stack_realloc_array(void *ptr, size_t old_count, size_t new_count, size_t element_size) {
    void *new_ptr = mem_stack_alloc(new_count * element_size);
    if (ptr && old_count > 0) {
        memcpy(new_ptr, ptr, old_count * element_size);
    }
    return new_ptr;
}
