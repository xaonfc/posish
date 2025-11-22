/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "error.h"
#include "jobs.h"

int builtin_jobid(char **args) {
    if (!args[1]) {
        fprintf(stderr, "jobid: usage: jobid jobspec\n");
        return 2;
    }
    
    pid_t pid = job_resolve_spec(args[1]);
    if (pid == -1) {
        error_msg("jobid: %s: no such job", args[1]);
        return 1;
    }
    
    printf("%d\n", pid);
    return 0;
}
