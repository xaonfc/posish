/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int builtin_export(char **args) {
    if (!args[1]) {
        // List exported variables
        char **env = posish_var_get_environ();
        for (char **e = env; *e != NULL; e++) {
            char *eq = strchr(*e, '=');
            if (eq) {
                *eq = '\0';
                printf("export %s=\"", *e);
                for (char *p = eq + 1; *p; p++) {
                    if (*p == '\\' || *p == '"' || *p == '$' || *p == '`') {
                        putchar('\\');
                    }
                    putchar(*p);
                }
                printf("\"\n");
                *eq = '='; // Restore for safety, though we free *e next
            } else {
                // Should not happen for environ entries, but just in case
                printf("export %s\n", *e);
            }
            free(*e);
        }
        free(env);
        return 0;
    }

    for (int i = 1; args[i] != NULL; i++) {
        char *arg = args[i];
        char *eq = strchr(arg, '=');
        if (eq) {
            *eq = '\0';
            posish_var_set(arg, eq + 1);
            posish_var_export(arg);
            *eq = '='; // Restore
        } else {
            posish_var_export(arg);
        }
    }
    return 0;
}
