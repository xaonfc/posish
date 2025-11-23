/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "functions.h"
#include <string.h>
#include <stdlib.h>

typedef struct Function {
    char *name;
    ASTNode *body;
    struct Function *next;
} Function;

static Function *functions = NULL;

void func_add(const char *name, ASTNode *body) {
    func_remove(name); // Overwrite if exists
    Function *f = malloc(sizeof(Function));
    f->name = strdup(name);
    f->body = body; 
    f->next = functions;
    functions = f;
}

ASTNode *func_get(const char *name) {
    for (Function *f = functions; f; f = f->next) {
        if (strcmp(f->name, name) == 0) return f->body;
    }
    return NULL;
}

void func_remove(const char *name) {
    Function **curr = &functions;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
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
