/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "alias.h"
#include <stdio.h>

int builtin_unalias(char **args) {
    if (!args[1]) {
        fprintf(stderr, "unalias: usage: unalias name [name ...]\n");
        return 1;
    }

    for (int i = 1; args[i] != NULL; i++) {
        alias_remove(args[i]);
    }
    return 0;
}
