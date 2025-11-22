/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "signals.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int builtin_trap(char **argv) {
    int argc = 0;
    while (argv[argc]) argc++;

    if (argc == 1) {
        signal_list_traps();
        return 0;
    }

    int arg_idx = 1;
    const char *action = NULL;
    int reset = 0;

    // Check if first argument is an integer (condition only, reset action)
    // POSIX: "If the first operand is an unsigned decimal integer, the shell shall treat all operands as conditions, and shall reset each condition to the default value."
    char *end;
    strtol(argv[arg_idx], &end, 10);
    if (*end == '\0' && isdigit(argv[arg_idx][0])) {
        // First operand is integer -> Reset all
        reset = 1;
    } else {
        // First operand is action
        action = argv[arg_idx++];
        if (strcmp(action, "-") == 0) {
            reset = 1;
        }
    }

    if (arg_idx >= argc) {
        // No conditions specified?
        // If action was specified but no conditions, what happens?
        // POSIX: "trap action condition..."
        // If only action is provided, it does nothing?
        return 0;
    }

    for (; arg_idx < argc; arg_idx++) {
        const char *cond = argv[arg_idx];
        int signum = signal_get_number(cond);
        
        if (signum == -1) {
            error_msg("trap: %s: invalid signal specification", cond);
            return 1;
        }

        if (reset) {
            if (signal_reset(signum) != 0) {
                error_msg("trap: %s: failed to reset signal", cond);
            }
        } else {
            if (signal_trap(signum, action) != 0) {
                error_msg("trap: %s: failed to set trap", cond);
            }
        }
    }

    return 0;
}
