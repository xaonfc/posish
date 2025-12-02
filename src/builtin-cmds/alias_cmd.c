/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "alias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int builtin_alias(char **args) {
    if (!args[1]) {
        alias_print_all();
        return 0;
    }

    for (int i = 1; args[i] != NULL; i++) {
        char *arg = args[i];
        char *eq = strchr(arg, '=');
        if (eq) {
            *eq = '\0';
            alias_add(arg, eq + 1);
            *eq = '='; // Restore
        } else {
            char *val = alias_get(arg);
            if (val) {
                printf("alias %s='%s'\n", arg, val);
                free(val);
            } else {
                fprintf(stderr, "alias: %s: not found\n", arg);
                return 1;
            }
        }
    }
    return 0;
}
