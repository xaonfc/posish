/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "variables.h"
#include <stddef.h>

int builtin_unset(char **args) {
    // args[0] is "unset"
    if (!args[1]) {
        return 0;
    }

    for (int i = 1; args[i] != NULL; i++) {
        posish_var_unset(args[i]);
    }
    return 0;
}
