/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "executor.h"
#include "error.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// We need to store the return value somewhere so the function call can return it.
// The return value of the builtin itself is EXIT_RETURN.
// The actual status code should be stored.
// We can use a global in executor or passing it via some mechanism.
// But wait, executor_execute returns the status.
// If we return EXIT_RETURN, we lose the value.
// Maybe EXIT_RETURN should be a base?
// Or we set "last_exit_status" before returning EXIT_RETURN?
// executor_execute sets last_exit_status to the return value of the builtin.
// So if we return EXIT_RETURN, last_exit_status becomes EXIT_RETURN.
// That's not what we want.
// We want the function call to return 'n'.

// Strategy:
// builtin_return sets a global "func_return_status" and returns EXIT_RETURN.
// The executor sees EXIT_RETURN, stops execution, and returns "func_return_status".

extern int func_return_status; // Defined in executor.c

int builtin_return(char **args) {
    // Default to last exit status? POSIX: "defaults to the exit status of the last command executed"
    // We need access to last_exit_status.
    // It's static in executor.c. We should expose getter/setter.
    
    // For now, let's assume 0 if not specified, or we need to expose last_exit_status.
    if (args[1]) {
        char *endptr;
        errno = 0; // Clear errno before call
        long val = strtol(args[1], &endptr, 10);
        
        if (errno == ERANGE || val > 255 || val < 0) { // Check for out of range or overflow/underflow
            fprintf(stderr, "posish: return: %s: numeric argument out of range (0-255)\n", args[1]);
            return 2; // POSIX specifies 2 for invalid argument to builtins
        }
        if (*endptr != '\0') {
            fprintf(stderr, "posish: return: %s: numeric argument required\n", args[1]);
            return 2; // POSIX specifies 2 for invalid argument to builtins
        }
        func_return_status = (int)val;
    } else {
        func_return_status = executor_get_last_status();
    }
    
    return EXIT_RETURN;
}
