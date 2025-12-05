/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"

#define EXIT_BREAK 100
#define EXIT_CONTINUE 101
#define EXIT_RETURN 102

extern int func_return_status;
extern int executor_break_count;
extern int executor_continue_count;
extern int executor_no_fork;

int executor_execute(struct ASTNode *node);
int executor_get_last_status(void);
void executor_set_last_status(int status);
char *find_executable(const char *command);


#endif
