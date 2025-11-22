/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include "builtins.h"
#include "error.h"
#include "jobs.h"

// Helper to parse signal name or number
static int get_signal(const char *str, int *sig) {
    if (!str) return -1;

    // Check if it's a number
    char *end;
    long val = strtol(str, &end, 10);
    if (*end == '\0') {
        *sig = (int)val;
        return 0;
    }

    // Check if it's a name (case insensitive)
    // We'll support common standard signals
    const char *names[] = {
        "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO", "PWR", "SYS", NULL
    };
    const int nums[] = {
        SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGFPE, SIGKILL, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM, SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGURG, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH, SIGIO, SIGPWR, SIGSYS
    };

    // Skip "SIG" prefix if present
    const char *s = str;
    if (strncasecmp(s, "SIG", 3) == 0) s += 3;

    for (int i = 0; names[i]; i++) {
        if (strcasecmp(s, names[i]) == 0) {
            *sig = nums[i];
            return 0;
        }
    }
    
    // Special case 0
    if (strcmp(s, "0") == 0) {
        *sig = 0;
        return 0;
    }

    return -1;
}

static void print_signals(void) {
    // Simple implementation: print standard signals
    // Ideally we should iterate over all supported signals
    // For now, let's print the ones we know
    const char *names[] = {
        "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO", "PWR", "SYS", NULL
    };
    
    for (int i = 0; names[i]; i++) {
        printf("%s%c", names[i], (i + 1) % 5 == 0 ? '\n' : ' ');
    }
    printf("\n");
}

static const char *get_signal_name(int sig) {
    const char *names[] = {
        "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO", "PWR", "SYS", NULL
    };
    const int nums[] = {
        SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGFPE, SIGKILL, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM, SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGURG, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH, SIGIO, SIGPWR, SIGSYS
    };
    
    for (int i = 0; names[i]; i++) {
        if (nums[i] == sig) return names[i];
    }
    return "UNKNOWN";
}

int builtin_kill(char **args) {
    int sig = SIGTERM; // Default signal
    int arg_idx = 1;
    int list_mode = 0;

    // Parse options
    if (args[1] && args[1][0] == '-') {
        if (strcmp(args[1], "-l") == 0) {
            list_mode = 1;
            arg_idx++;
        } else if (strcmp(args[1], "-s") == 0) {
            if (args[2]) {
                if (get_signal(args[2], &sig) != 0) {
                    error_msg("kill: %s: invalid signal specification", args[2]);
                    return 1;
                }
                arg_idx += 2;
            } else {
                error_msg("kill: -s requires an argument");
                return 2;
            }
        } else if (strcmp(args[1], "--") == 0) {
            arg_idx++;
        } else {
            // Check for -SIGNAL
            if (get_signal(args[1] + 1, &sig) == 0) {
                arg_idx++;
            } else {
                // Could be negative PID if it's a number, but POSIX says options come first.
                // However, "kill -123" could be signal 123 OR pid -123?
                // POSIX: "If the first argument is a negative integer, it shall be interpreted as a -signal_number option"
                // So -123 is signal 123.
                // But -TERM is signal TERM.
                // If get_signal fails, it might be an invalid option.
                error_msg("kill: %s: invalid option or signal", args[1]);
                return 2;
            }
        }
    }

    if (list_mode) {
        if (args[arg_idx]) {
            // Print signal name for exit status
            int status = atoi(args[arg_idx]);
            if (status > 128) status -= 128;
            printf("%s\n", get_signal_name(status));
        } else {
            print_signals();
        }
        return 0;
    }

    if (!args[arg_idx]) {
        error_msg("kill: usage: kill [-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill -l [exit_status]");
        return 2;
    }

    int status = 0;
    for (; args[arg_idx]; arg_idx++) {
        char *target = args[arg_idx];
        pid_t pid;

        if (target[0] == '%') {
            pid = job_resolve_spec(target);
            if (pid == -1) {
                error_msg("kill: %s: no such job", target);
                status = 1;
                continue;
            }
        } else {
            pid = atoi(target);
        }

        if (kill(pid, sig) < 0) {
            error_sys("kill: (%s) - %d", target, pid);
            status = 1;
        }
    }

    return status;
}
