/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include <string.h>

int builtin_is_builtin(const char *name) {
    if (strcmp(name, "cd") == 0) return 1;
    if (strcmp(name, "echo") == 0) return 1;
    if (strcmp(name, "exit") == 0) return 1;
    if (strcmp(name, "export") == 0) return 1;
    if (strcmp(name, "unset") == 0) return 1;
    if (strcmp(name, "set") == 0) return 1;
    if (strcmp(name, "jobs") == 0) return 1;
    if (strcmp(name, "fg") == 0) return 1;
    if (strcmp(name, "bg") == 0) return 1;
    if (strcmp(name, "alias") == 0) return 1;
    if (strcmp(name, "unalias") == 0) return 1;
    if (strcmp(name, "test") == 0) return 1;
    if (strcmp(name, "[") == 0) return 1;
    if (strcmp(name, ".") == 0) return 1;
    if (strcmp(name, "printf") == 0) return 1;
    if (strcmp(name, "eval") == 0) return 1;
    if (strcmp(name, "exec") == 0) return 1;
    if (strcmp(name, "read") == 0) return 1;
    if (strcmp(name, "kill") == 0) return 1;
    if (strcmp(name, "break") == 0) return 1;
    if (strcmp(name, "continue") == 0) return 1;
    if (strcmp(name, "return") == 0) return 1;
    if (strcmp(name, "shift") == 0) return 1;
    if (strcmp(name, "wait") == 0) return 1;
    if (strcmp(name, "pwd") == 0) return 1;
    if (strcmp(name, "type") == 0) return 1;
    if (strcmp(name, "trap") == 0) return 1;
    if (strcmp(name, "umask") == 0) return 1;
    if (strcmp(name, "times") == 0) return 1;
    if (strcmp(name, "command") == 0) return 1;
    if (strcmp(name, "readonly") == 0) return 1;
    if (strcmp(name, "getopts") == 0) return 1;
    if (strcmp(name, ":") == 0) return 1;
    if (strcmp(name, "true") == 0) return 1;
    if (strcmp(name, "false") == 0) return 1;
    if (strcmp(name, "false") == 0) return 1;
    if (strcmp(name, "local") == 0) return 1;
    if (strcmp(name, "typeset") == 0) return 1;
    return 0;
}

int builtin_wait(char **argv);
int builtin_trap(char **argv);
int builtin_umask(char **argv);

int builtin_umask(char **argv);
int builtin_times(char **argv);
int builtin_command(char **argv);
int builtin_readonly(char **argv);
int builtin_getopts(char **argv);
int builtin_true(char **argv);
int builtin_false(char **argv);
int builtin_colon(char **argv);
int builtin_local(char **argv);
int builtin_typeset(char **argv);

int builtin_run(char **args) {
    if (strcmp(args[0], "cd") == 0) return builtin_cd(args);
    if (strcmp(args[0], "echo") == 0) return builtin_echo(args);
    if (strcmp(args[0], "exit") == 0) return builtin_exit(args);
    if (strcmp(args[0], "export") == 0) return builtin_export(args);
    if (strcmp(args[0], "unset") == 0) return builtin_unset(args);
    if (strcmp(args[0], "set") == 0) return builtin_set(args);
    if (strcmp(args[0], "jobs") == 0) return builtin_jobs(args);
    if (strcmp(args[0], "fg") == 0) return builtin_fg(args);
    if (strcmp(args[0], "bg") == 0) return builtin_bg(args);
    if (strcmp(args[0], "alias") == 0) return builtin_alias(args);
    if (strcmp(args[0], "unalias") == 0) return builtin_unalias(args);
    if (strcmp(args[0], "test") == 0) return builtin_test(args);
    if (strcmp(args[0], "[") == 0) return builtin_test(args);
    if (strcmp(args[0], ".") == 0) return builtin_dot(args);
    if (strcmp(args[0], "printf") == 0) return builtin_printf(args);
    if (strcmp(args[0], "eval") == 0) return builtin_eval(args);
    if (strcmp(args[0], "exec") == 0) return builtin_exec(args);
    if (strcmp(args[0], "read") == 0) return builtin_read(args);
    if (strcmp(args[0], "kill") == 0) return builtin_kill(args);
    if (strcmp(args[0], "break") == 0) return builtin_break(args);
    if (strcmp(args[0], "continue") == 0) return builtin_continue(args);
    if (strcmp(args[0], "return") == 0) return builtin_return(args);
    if (strcmp(args[0], "shift") == 0) return builtin_shift(args);
    if (strcmp(args[0], "wait") == 0) return builtin_wait(args);
    if (strcmp(args[0], "pwd") == 0) return builtin_pwd(args);
    if (strcmp(args[0], "type") == 0) return builtin_type(args);
    if (strcmp(args[0], "trap") == 0) return builtin_trap(args);
    if (strcmp(args[0], "umask") == 0) return builtin_umask(args);
    if (strcmp(args[0], "times") == 0) return builtin_times(args);
    if (strcmp(args[0], "command") == 0) return builtin_command(args);
    if (strcmp(args[0], "readonly") == 0) return builtin_readonly(args);
    if (strcmp(args[0], "getopts") == 0) return builtin_getopts(args);
    if (strcmp(args[0], ":") == 0) return builtin_colon(args);
    if (strcmp(args[0], "true") == 0) return builtin_true(args);
    if (strcmp(args[0], "false") == 0) return builtin_false(args);
    if (strcmp(args[0], "local") == 0) return builtin_local(args);
    if (strcmp(args[0], "typeset") == 0) return builtin_typeset(args);
    return 1;
}
