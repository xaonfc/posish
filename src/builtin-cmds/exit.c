/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "signals.h"
#include <stdlib.h>
#include <stdio.h>

int builtin_exit(char **args) {
    int status = 0;
    if (args[1] != NULL) {
        status = atoi(args[1]);
    }
    signal_trigger_exit();
    fflush(NULL);
    exit(status);
    return 0; // Unreachable
}
