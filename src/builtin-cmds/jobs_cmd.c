/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "jobs.h"

int builtin_jobs(char **args) {
    (void)args;
    job_print_all();
    return 0;
}
