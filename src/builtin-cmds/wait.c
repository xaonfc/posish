/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "jobs.h"
#include "error.h"
#include <stdlib.h>
#include <sys/wait.h>

int builtin_wait(char **args) {
    if (!args[1]) {
        return job_wait_all();
    }
    
    int status = 0;
    for (int i = 1; args[i]; i++) {
        pid_t pid = -1;
        if (args[i][0] == '%') {
            pid = job_resolve_spec(args[i]);
        } else {
            pid = atoi(args[i]);
        }
        
        if (pid <= 0) {
            error_msg("wait: %s: invalid job spec or pid", args[i]);
            status = 127;
            continue;
        }
        
        Job *j = job_find_by_pid(pid);
        if (j) {
            status = job_wait(j);
        } else {
            // If not found, maybe it's already done?
            // POSIX says if pid is not a child of this shell, return 127.
            // But we don't track all history.
            // For now, assume 127.
            error_msg("wait: pid %d is not a child of this shell", pid);
            status = 127;
        }
    }
    return status;
}
