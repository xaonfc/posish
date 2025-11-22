/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int builtin_bg(char **args) {
    int job_id = -1;
    if (args[1]) {
        if (args[1][0] == '%') {
            job_id = atoi(args[1] + 1);
        } else {
            job_id = atoi(args[1]);
        }
    } else {
        fprintf(stderr, "bg: job id required (e.g. %%1)\n");
        return 1;
    }

    Job *j = job_find_by_id(job_id);
    if (!j) {
        fprintf(stderr, "bg: %d: no such job\n", job_id);
        return 1;
    }

    if (j->status == JOB_RUNNING) {
        fprintf(stderr, "bg: job %d already running\n", job_id);
        return 0;
    }

    printf("[%d]+ %s &\n", j->id, j->command);
    kill(-j->pgid, SIGCONT);
    j->status = JOB_RUNNING;

    return 0;
}
