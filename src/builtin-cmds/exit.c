/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "signals.h"
#include <stdlib.h>
#include <stdio.h>
#include "error.h"
#include <ctype.h>

int builtin_exit(char **args) {
    int status = 0;
    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        
        // Check if valid number
        if (*endptr != '\0' || args[1] == endptr) {
            error_msg("exit: %s: numeric argument required", args[1]);
            status = 2; // Bash uses 2 for syntax error/invalid arg
        } else {
            status = (int)val;
        }
        
        if (args[2] != NULL) {
            error_msg("exit: too many arguments");
            return 1; // Don't exit
        }
    }
    signal_trigger_exit();
    fflush(NULL);
    exit(status);
    return 0; // Unreachable
}
