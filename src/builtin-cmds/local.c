/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <string.h>
#include "builtins.h"
#include "variables.h"

int builtin_local(char **argv) {
    // local var1 var2=value var3
    for (int i = 1; argv[i]; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            // Has assignment: local var=value
            *eq = '\0';
            var_declare_local(argv[i], eq + 1);
            *eq = '='; // Restore for potential reuse
        } else {
            // Just declaration: local var
            var_declare_local(argv[i], "");
        }
    }
    return 0;
}

