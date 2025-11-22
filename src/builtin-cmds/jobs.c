/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "jobs.h"
#include <stdio.h>

int builtin_jobs(char **args) {
    (void)args; // Unused for now
    job_print_all();
    return 0;
}
