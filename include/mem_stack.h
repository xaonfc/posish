/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef MEM_STACK_H
#define MEM_STACK_H

#include <stddef.h>

/*
 * Stack-based memory allocator (Region Allocator).
 * Modeled after dash's stalloc.
 *
 * Usage:
 * - mem_stack_alloc(size): Allocate memory from the current block.
 * - mem_stack_push_mark(mark): Save current stack state.
 * - mem_stack_pop_mark(mark): Restore stack state (freeing memory).
 * - mem_stack_str_*: Efficient string building.
 */

struct stack_block;

struct stackmark {
    struct stack_block *stackp;
    char *stacknxt;
    size_t stacknleft;
};

/* Global stack state (for performance, similar to dash) */
extern char *stacknxt;
extern size_t stacknleft;

/* Core API */
void *mem_stack_alloc(size_t nbytes);
char *mem_stack_strdup(const char *s);
void mem_stack_push_mark(struct stackmark *mark);
void mem_stack_pop_mark(struct stackmark *mark);
void *mem_stack_realloc_array(void *ptr, size_t old_count, size_t new_count, size_t element_size);

/* String building API */
void *mem_stack_grow_str(void);
char *mem_stack_grow_to(size_t len);
char *mem_stack_make_space(size_t n, char *p);
char *mem_stack_put_s(const char *s, char *p);

/* Macros for efficient character pushing */
#define MEM_STACK_PUTC(c, p) ((p) = _mem_stack_putc((c), (p)))

static inline char *_mem_stack_putc(int c, char *p) {
    if (stacknleft == 0) {
        p = mem_stack_grow_str();
    }
    *p++ = c;
    stacknleft--;
    stacknxt = p;
    return p;
}

/* Helper to get current string start */
#define mem_stack_block() ((void *)stacknxt)

/* Helper to grab the string (finalize it) */
void *mem_stack_grab_str(char *p);

#endif /* MEM_STACK_H */
