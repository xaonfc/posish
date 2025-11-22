/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef VARIABLES_H
#define VARIABLES_H

#include <sys/types.h>

void var_init(char **envp);
void var_set(const char *name, const char *value);
char *var_get(const char *name);
void var_unset(const char *name);
void var_export(const char *name);
char **var_get_environ(void);
char **var_get_all(void);
int var_is_valid_name(const char *name);
void var_set_readonly(const char *name);
int var_is_readonly(const char *name);
char **var_get_all_readonly(void);

void var_set_positional(int argc, char **argv);
char *var_get_positional(int index);
int var_get_positional_count(void);
char **var_get_all_positional(void);
int var_shift_positional(int n);
char **var_get_positional_params(size_t *count);

void var_set_last_bg_pid(pid_t pid);
pid_t var_get_last_bg_pid(void);
void var_set_shell_name(const char *name);
char *var_get_shell_name(void);

void var_push_scope(void);
void var_pop_scope(void);
void var_declare_local(const char *name, const char *value);

#endif
