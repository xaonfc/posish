/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,
    JOB_TERMINATED
} JobStatus;

typedef struct Job {
    int id;
    pid_t pgid;
    char *command;
    JobStatus status;
    struct Job *next;
} Job;

void job_init(void);
Job *job_add(pid_t pgid, const char *command, JobStatus status);
void job_remove(int id);
Job *job_find_by_pid(pid_t pgid);
Job *job_find_by_id(int id);
void job_print_all(void);
void job_update_status(pid_t pgid, JobStatus status);
void job_update_status(pid_t pgid, JobStatus status);
int job_get_next_id(void);
pid_t job_resolve_spec(const char *spec);
int job_wait(Job *j);
int job_wait_all(void);

#endif
