/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include <stdio.h>

int builtin_echo(char **args) {
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i+1] != NULL) {
            printf(" ");
        }
    }
    printf("\n");
    fflush(stdout);
    return 0;
}
