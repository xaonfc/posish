/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef BUILTINS_H
#define BUILTINS_H

int builtin_is_builtin(const char *name);
int builtin_run(char **args);

// Individual builtins
int builtin_cd(char **args);
int builtin_echo(char **args);
int builtin_exit(char **args);
int builtin_export(char **args);
int builtin_unset(char **args);
int builtin_set(char **args);
int builtin_jobs(char **args);
int builtin_bg(char **args);
int builtin_fg(char **args);
int builtin_alias(char **args);
int builtin_unalias(char **args);
int builtin_test(char **args);
int builtin_dot(char **argv);
int builtin_printf(char **argv);
int builtin_eval(char **argv);
int builtin_exec(char **args);
int builtin_read(char **args);
int builtin_kill(char **args);
int builtin_break(char **args);
int builtin_continue(char **args);
int builtin_return(char **args);
int builtin_shift(char **args);
int builtin_wait(char **args);
int builtin_pwd(char **args);
int builtin_type(char **args);

#endif
