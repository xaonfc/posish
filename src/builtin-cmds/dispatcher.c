/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "builtins.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    const char *name;
    int (*func)(char **);
} Builtin;

int builtin_wait(char **argv);
int builtin_trap(char **argv);
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
int testcmd(char **argv);

/* Sorted array of builtins for binary search */
static const Builtin builtins[] = {
    {".", builtin_dot},
    {":", builtin_colon},
    {"[", testcmd},
    {"alias", builtin_alias},
    {"bg", builtin_bg},
    {"break", builtin_break},
    {"cd", builtin_cd},
    {"command", builtin_command},
    {"continue", builtin_continue},
    {"echo", builtin_echo},
    {"eval", builtin_eval},
    {"exec", builtin_exec},
    {"exit", builtin_exit},
    {"export", builtin_export},
    {"false", builtin_false},
    {"fg", builtin_fg},
    {"getopts", builtin_getopts},
    {"jobs", builtin_jobs},
    {"kill", builtin_kill},
    {"local", builtin_local},
    {"printf", builtin_printf},
    {"pwd", builtin_pwd},
    {"read", builtin_read},
    {"readonly", builtin_readonly},
    {"return", builtin_return},
    {"set", builtin_set},
    {"shift", builtin_shift},
    {"test", testcmd},
    {"times", builtin_times},
    {"trap", builtin_trap},
    {"true", builtin_true},
    {"type", builtin_type},
    {"typeset", builtin_typeset},
    {"umask", builtin_umask},
    {"unalias", builtin_unalias},
    {"unset", builtin_unset},
    {"wait", builtin_wait},
};

static int compare_builtins(const void *a, const void *b) {
    const char *key = a;
    const Builtin *entry = b;
    return strcmp(key, entry->name);
}

int builtin_is_builtin(const char *name) {
    return bsearch(name, builtins, sizeof(builtins) / sizeof(Builtin), sizeof(Builtin), compare_builtins) != NULL;
}

int builtin_run(char **args) {
    Builtin *entry = bsearch(args[0], builtins, sizeof(builtins) / sizeof(Builtin), sizeof(Builtin), compare_builtins);
    if (entry) {
        return entry->func(args);
    }
    return 127; // Should not happen if checked with builtin_is_builtin
}

