/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef MEMALLOC_H
#define MEMALLOC_H

#include <stddef.h>

/* Safe malloc wrappers */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

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

/* Helper to get current string start */
#define mem_stack_block() ((void *)stacknxt)

/* Helper to grab the string (finalize it) */
void *mem_stack_grab_str(char *p);

#endif
