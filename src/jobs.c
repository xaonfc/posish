/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <errno.h>

static Job *jobs = NULL;
static int next_job_id = 1;

void job_init(void) {
    jobs = NULL;
    next_job_id = 1;
}

Job *job_add(pid_t pgid, const char *command, JobStatus status) {
    Job *j = malloc(sizeof(Job));
    j->id = next_job_id++;
    j->pgid = pgid;
    j->command = strdup(command);
    j->status = status;
    j->next = NULL;

    if (!jobs) {
        jobs = j;
    } else {
        Job *last = jobs;
        while (last->next) {
            last = last->next;
        }
        last->next = j;
    }
    return j;
}

void job_remove(int id) {
    Job *curr = jobs;
    Job *prev = NULL;
    while (curr) {
        if (curr->id == id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                jobs = curr->next;
            }
            free(curr->command);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

Job *job_find_by_pid(pid_t pgid) {
    Job *j = jobs;
    while (j) {
        if (j->pgid == pgid) return j;
        j = j->next;
    }
    return NULL;
}

Job *job_find_by_id(int id) {
    Job *j = jobs;
    while (j) {
        if (j->id == id) return j;
        j = j->next;
    }
    return NULL;
}

void job_print_all(void) {
    Job *j = jobs;
    while (j) {
        const char *status_str = "Unknown";
        switch (j->status) {
            case JOB_RUNNING: status_str = "Running"; break;
            case JOB_STOPPED: status_str = "Stopped"; break;
            case JOB_DONE: status_str = "Done"; break;
            case JOB_TERMINATED: status_str = "Terminated"; break;
        }
        printf("[%d] %s %s\n", j->id, status_str, j->command);
        j = j->next;
    }
}

void job_update_status(pid_t pgid, JobStatus status) {
    Job *j = job_find_by_pid(pgid);
    if (j) {
        j->status = status;
    }
}

int job_get_next_id(void) {
    return next_job_id;
}

pid_t job_resolve_spec(const char *spec) {
    if (!spec || *spec != '%') return -1;
    
    spec++; // Skip '%'
    
    if (*spec == '\0' || (*spec == '%' && spec[1] == '\0') || (*spec == '+' && spec[1] == '\0')) {
        // Current job (%% or %+)
        // For now, just return the last job added or updated
        // Ideally we should track "current" and "previous" jobs
        if (jobs) {
            Job *j = jobs;
            while (j->next) j = j->next;
            return j->pgid;
        }
        return -1;
    }
    
    if (isdigit(*spec)) {
        int id = atoi(spec);
        Job *j = job_find_by_id(id);
        if (j) return j->pgid;
    }
    
    // TODO: Handle string search (?string)
    
    return -1;
}

int job_wait(Job *j) {
    if (!j) return -1;
    
    int status = 0;
    pid_t pid;
    
    // Wait for the process group
    while ((pid = waitpid(-j->pgid, &status, 0)) < 0) {
        if (errno == EINTR) continue;
        
        // Try waiting for the process itself if pgid fails
        while ((pid = waitpid(j->pgid, &status, 0)) < 0) {
            if (errno == EINTR) continue;
            perror("waitpid");
            return -1;
        }
        break;
    }
    
    if (WIFEXITED(status)) {
        j->status = JOB_DONE;
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        j->status = JOB_TERMINATED;
        return 128 + WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        j->status = JOB_STOPPED;
        return 128 + WSTOPSIG(status);
    }
    
    return 0;
}

int job_wait_all(void) {
    Job *j = jobs;
    int last_status = 0;
    while (j) {
        if (j->status == JOB_RUNNING) {
            last_status = job_wait(j);
        }
        j = j->next;
    }
    return last_status;
}
