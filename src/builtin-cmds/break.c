/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "executor.h"
#include "error.h"
#include <stdlib.h>
#include <stdio.h> // Added for fprintf

int builtin_break(char **args) {
    int n = 1;
    if (args[1]) {
        char *endptr;
        n = (int)strtol(args[1], &endptr, 10);
        if (*endptr != '\0' || n <= 0) {
            fprintf(stderr, "posish: break: %s: numeric argument required\n", args[1]);
            return 128; // Non-zero exit status for error
        }
    }
    // If n=2, return -3? No, EXIT_CONTINUE is -3.
    
    // Let's use a global variable in executor.c or variables.c to store the break/continue level.
    // For simplicity, let's just support break 1 for now or use a global.
    // Actually, POSIX says "If n is specified, the break utility shall exit from n enclosing loops."
    
    // Let's assume we set a global "loop_control_level" in executor.
    // But we are in builtin-cmds.
    // We can add a function in executor.h to set this.
    
    // For now, let's just return EXIT_BREAK. 
    // To support 'n', we might need to extend the protocol.
    // Let's stick to simple break for this iteration or add a global.
    
    executor_break_count = n;
    return EXIT_BREAK;
}
