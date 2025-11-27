/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "shell_options.h"

int shell_trace_mode = 0;
int shell_exit_on_error = 0;
int shell_no_glob = 0;
int shell_no_clobber = 0;
int shell_no_unset = 0;
int shell_verbose = 0;
int shell_no_exec = 0;
int shell_all_export = 0;
int shell_monitor = 0;
int shell_hash_all = 0;
int shell_notify = 0;
int shell_ignore_eof = 0;
int shell_nolog = 0;
int shell_vi_mode = 0;
int shell_ignore_errexit = 0;

void shell_options_init(void) {
    shell_trace_mode = 0;
    shell_exit_on_error = 0;
    shell_no_glob = 0;
    shell_no_clobber = 0;
    shell_no_unset = 0;
    shell_verbose = 0;
    shell_no_exec = 0;
    shell_all_export = 0;
    shell_monitor = 0;
    shell_hash_all = 0;
    shell_notify = 0;
    shell_ignore_eof = 0;
    shell_nolog = 0;
    shell_vi_mode = 0;
    shell_ignore_errexit = 0;
}
