/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "executor.h"
#include "error.h"
#include "alias.h"
#include "functions.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int builtin_type(char **args) {
    if (!args[1]) {
        return 0;
    }
    
    int status = 0;
    for (int i = 1; args[i]; i++) {
        const char *name = args[i];
        
        // Check alias
        char *alias = alias_get(name);
        if (alias) {
            printf("%s is an alias for %s\n", name, alias);
            free(alias);
            continue;
        }
        
        // Check builtin
        if (builtin_is_builtin(name)) {
            printf("%s is a shell builtin\n", name);
            continue;
        }
        
        // Check function
        if (func_get(name)) {
            printf("%s is a function\n", name);
            continue;
        }
        
        // Check path
        char *path = find_executable(name);
        if (path) {
            printf("%s is %s\n", name, path);
            free(path);
            continue;
        }
        
        error_msg("type: %s: not found", name);
        status = 1;
    }
    return status;
}
