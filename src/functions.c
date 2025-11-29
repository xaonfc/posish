/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "functions.h"
#include <string.h>
#include <stdlib.h>

#define FUNC_HASH_SIZE 128

typedef struct Function {
    char *name;
    size_t name_len;
    ASTNode *body;
    struct Function *next;
} Function;

static Function *func_tab[FUNC_HASH_SIZE] = {NULL};

static unsigned long hash_djb2(const char *str, size_t *len_out) {
    unsigned long hash = 5381;
    int c;
    size_t len = 0;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
        len++;
    }
    if (len_out) *len_out = len;
    return hash % FUNC_HASH_SIZE;
}

void func_add(const char *name, ASTNode *body) {
    size_t len;
    unsigned long h = hash_djb2(name, &len);
    
    // Check if exists and update
    Function *f = func_tab[h];
    while (f) {
        if (f->name_len == len && strcmp(f->name, name) == 0) {
            ast_free_heap(f->body);
            f->body = body;
            return;
        }
        f = f->next;
    }
    
    // Add new
    f = malloc(sizeof(Function));
    f->name = strdup(name);
    f->name_len = len;
    f->body = body;
    f->next = func_tab[h];
    func_tab[h] = f;
}

ASTNode *func_get(const char *name) {
    size_t len;
    unsigned long h = hash_djb2(name, &len);
    
    Function *f = func_tab[h];
    while (f) {
        if (f->name_len == len && strcmp(f->name, name) == 0) {
            return f->body;
        }
        f = f->next;
    }
    return NULL;
}

void func_remove(const char *name) {
    size_t len;
    unsigned long h = hash_djb2(name, &len);
    
    Function **curr = &func_tab[h];
    while (*curr) {
        if ((*curr)->name_len == len && strcmp((*curr)->name, name) == 0) {
            Function *temp = *curr;
            *curr = (*curr)->next;
            free(temp->name);
            ast_free_heap(temp->body);
            free(temp);
            return;
        }
        curr = &(*curr)->next;
    }
}
