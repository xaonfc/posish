/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef SHELL_OPTIONS_H
#define SHELL_OPTIONS_H

// Global shell options
// Global shell options
extern int shell_trace_mode;      // set -x
extern int shell_exit_on_error;   // set -e
extern int shell_no_glob;         // set -f
extern int shell_no_clobber;      // set -C
extern int shell_no_unset;        // set -u
extern int shell_verbose;         // set -v
extern int shell_no_exec;         // set -n
extern int shell_all_export;      // set -a
extern int shell_monitor;         // set -m
extern int shell_hash_all;        // set -h
extern int shell_notify;          // set -b
extern int shell_ignore_errexit;  // Internal flag to ignore -e

void shell_options_init(void);

#endif
