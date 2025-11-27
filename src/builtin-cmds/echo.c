/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include <stdio.h>
#include <string.h>

int builtin_echo(char **args) {
    int suppress_newline = 0;
    int start_idx = 1;
    
    // Check for -n option
    if (args[1] && strcmp(args[1], "-n") == 0) {
        suppress_newline = 1;
        start_idx = 2;
    }
    
    for (int i = start_idx; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i+1] != NULL) {
            printf(" ");
        }
    }
    if (!suppress_newline) {
        printf("\n");
    }
    fflush(stdout);
    return 0;
}
