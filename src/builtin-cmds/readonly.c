/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "variables.h"

int builtin_readonly(char **argv) {
    // No arguments: list all readonly variables
    if (!argv[1]) {
        char **readonly_vars = posish_var_get_all_readonly();
        for (int i = 0; readonly_vars[i]; i++) {
            printf("readonly %s\n", readonly_vars[i]);
            free(readonly_vars[i]);
        }
        free(readonly_vars);
        return 0;
    }
    
    // Mark variables as readonly
    for (int i = 1; argv[i]; i++) {
        char *eq = strchr(argv[i], '=');
        
        if (eq) {
            // readonly VAR=value
            *eq = '\0';
            const char *name = argv[i];
            const char *value = eq + 1;
            
            if (posish_var_is_readonly(name)) {
                fprintf(stderr, "readonly: %s: readonly variable\n", name);
                *eq = '='; // Restore
                continue;
            }
            
            posish_var_set(name, value);
            posish_var_set_readonly(name);
            *eq = '='; // Restore
        } else {
            // readonly VAR (mark existing as readonly)
            const char *name = argv[i];
            
            if (!posish_var_get(name)) {
                fprintf(stderr, "readonly: %s: not found\n", name);
                continue;
            }
            
            posish_var_set_readonly(name);
        }
    }
    
    return 0;
}
