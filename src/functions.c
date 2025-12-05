/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "functions.h"
#include "ast.h"
#include "memalloc.h"

#include <string.h>

/* ============================================================================
 * Hash Table Configuration
 * ============================================================================ */

#define FUNC_HASH_SIZE 128

typedef struct Function {
    char           *name;
    size_t          name_len;
    ASTNode        *body;
    struct Function *next;
} Function;

static Function *func_table[FUNC_HASH_SIZE];

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static unsigned long hash_name(const char *name) {
    unsigned long hash = 5381;
    for (const char *p = name; *p; p++) {
        hash = ((hash << 5) + hash) + (unsigned char)*p;
    }
    return hash % FUNC_HASH_SIZE;
}

static Function *find_function(const char *name, size_t len, unsigned long h) {
    for (Function *f = func_table[h]; f; f = f->next) {
        if (f->name_len == len && memcmp(f->name, name, len) == 0) {
            return f;
        }
    }
    return NULL;
}

static void free_function(Function *f) {
    free(f->name);
    ast_free_heap(f->body);
    free(f);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void func_define(const char *name, ASTNode *body) {
    size_t len = strlen(name);
    unsigned long h = hash_name(name);

    Function *f = find_function(name, len, h);
    if (f) {
        ast_free_heap(f->body);
        f->body = body;
        return;
    }

    f = xmalloc(sizeof(Function));
    *f = (Function){
        .name     = xstrdup(name),
        .name_len = len,
        .body     = body,
        .next     = func_table[h],
    };
    func_table[h] = f;
}

ASTNode *func_lookup(const char *name) {
    size_t len = strlen(name);
    unsigned long h = hash_name(name);

    Function *f = find_function(name, len, h);
    return f ? f->body : NULL;
}

bool func_unset(const char *name) {
    size_t len = strlen(name);
    unsigned long h = hash_name(name);

    Function **slot = &func_table[h];
    while (*slot) {
        Function *f = *slot;
        if (f->name_len == len && memcmp(f->name, name, len) == 0) {
            *slot = f->next;
            free_function(f);
            return true;
        }
        slot = &f->next;
    }
    return false;
}

void func_clear_all(void) {
    for (size_t i = 0; i < FUNC_HASH_SIZE; i++) {
        Function *f = func_table[i];
        while (f) {
            Function *next = f->next;
            free_function(f);
            f = next;
        }
        func_table[i] = NULL;
    }
}

void func_foreach(void (*callback)(const char *name, ASTNode *body, void *ctx), void *ctx) {
    for (size_t i = 0; i < FUNC_HASH_SIZE; i++) {
        for (Function *f = func_table[i]; f; f = f->next) {
            callback(f->name, f->body, ctx);
        }
    }
}
