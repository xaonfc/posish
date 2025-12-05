/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdbool.h>
#include "ast.h"

// Define or replace a function. Takes ownership of `body`.
void func_define(const char *name, ASTNode *body);

// Lookup a function body by name, or return NULL if not found.
ASTNode *func_lookup(const char *name);

// Unset a function. Returns true if it existed and was removed.
bool func_unset(const char *name);

// Remove all functions.
void func_clear_all(void);

// Iterate over all functions.
void func_foreach(
    void (*callback)(const char *name, ASTNode *body, void *ctx),
    void *ctx
);

#endif /* FUNCTIONS_H */
