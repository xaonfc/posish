/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "error.h"

int builtin_exec(char **args) {
    // If no arguments (just "exec"), return 0.
    // Redirections are handled by the caller (executor).
    if (!args[1]) {
        return 0;
    }

    // Replace the shell process
    execvp(args[1], &args[1]);
    
    // If execvp returns, it failed
    error_sys("exec: %s", args[1]);
    return 126; // Command invoked cannot execute
}
