/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "ast.h"

void func_add(const char *name, ASTNode *body);
ASTNode *func_get(const char *name);
void func_remove(const char *name);

#endif
