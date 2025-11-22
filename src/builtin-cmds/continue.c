/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "executor.h"
#include "error.h"
#include <stdlib.h>
#include <stdio.h> // Added for fprintf

int builtin_continue(char **args) {
    int n = 1;
    if (args[1]) {
        char *endptr;
        n = (int)strtol(args[1], &endptr, 10);
        if (*endptr != '\0' || n <= 0) {
            fprintf(stderr, "posish: continue: %s: numeric argument required\n", args[1]);
            return 128;
        }
    }
    
    executor_continue_count = n;
    return EXIT_CONTINUE;
}
