/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>

int builtin_fg(char **args) {
    int job_id = -1;
    if (args[1]) {
        if (args[1][0] == '%') {
            job_id = atoi(args[1] + 1);
        } else {
            job_id = atoi(args[1]);
        }
    } else {
        // Default to last job (not implemented tracking of 'current' job yet, so just take max ID)
        // For now, error if no args
        fprintf(stderr, "fg: job id required (e.g. %%1)\n");
        return 1;
    }

    Job *j = job_find_by_id(job_id);
    if (!j) {
        fprintf(stderr, "fg: %d: no such job\n", job_id);
        return 1;
    }

    printf("%s\n", j->command);

    // Give terminal to job
    tcsetpgrp(STDIN_FILENO, j->pgid);

    // Continue job if stopped
    if (j->status == JOB_STOPPED) {
        kill(-j->pgid, SIGCONT);
    }
    
    j->status = JOB_RUNNING;

    // Wait for job
    int status;
    waitpid(-j->pgid, &status, WUNTRACED);

    // Restore terminal to shell
    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (WIFSTOPPED(status)) {
        j->status = JOB_STOPPED;
        printf("\n[%d]+  Stopped                 %s\n", j->id, j->command);
    } else {
        // Job done
        job_remove(j->id);
    }

    return 0;
}
