/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "signals.h"
#include "executor.h"
#include "memalloc.h"
#include "lexer.h"
#include "parser.h"
#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#include <strings.h>

#define MAX_SIGNALS 64

static char *trap_commands[MAX_SIGNALS];
static volatile sig_atomic_t pending_signals[MAX_SIGNALS];
static volatile sig_atomic_t any_pending_signal = 0;
static int signals_ignored_on_entry[MAX_SIGNALS];

// For interactive mode SIGINT handling
volatile sig_atomic_t got_sigint = 0;

// Mapping for signal names
typedef struct {
    const char *name;
    int number;
} SignalMap;

static const SignalMap signal_map[] = {
    {"EXIT", 0},
    {"HUP", SIGHUP},
    {"INT", SIGINT},
    {"QUIT", SIGQUIT},
    {"ILL", SIGILL},
    {"TRAP", SIGTRAP},
    {"ABRT", SIGABRT},
    {"BUS", SIGBUS},
    {"FPE", SIGFPE},
    {"KILL", SIGKILL},
    {"USR1", SIGUSR1},
    {"SEGV", SIGSEGV},
    {"USR2", SIGUSR2},
    {"PIPE", SIGPIPE},
    {"ALRM", SIGALRM},
    {"TERM", SIGTERM},
    {"CHLD", SIGCHLD},
    {"CONT", SIGCONT},
    {"STOP", SIGSTOP},
    {"TSTP", SIGTSTP},
    {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU},
    {"URG", SIGURG},
    {"XCPU", SIGXCPU},
    {"XFSZ", SIGXFSZ},
    {"VTALRM", SIGVTALRM},
    {"PROF", SIGPROF},
    {"WINCH", SIGWINCH},
    {NULL, 0}
};

static void handler(int signum) {
    // Special handling for SIGINT in interactive mode
    if (signum == SIGINT && !trap_commands[SIGINT]) {
        got_sigint = 1;
    }
    
    if (signum > 0 && signum < MAX_SIGNALS) {
        pending_signals[signum] = 1;
        any_pending_signal = 1;
    }
}

void signal_init(void) {
    struct sigaction sa;
    
    for (int i = 0; i < MAX_SIGNALS; i++) {
        trap_commands[i] = NULL;
        pending_signals[i] = 0;
        signals_ignored_on_entry[i] = 0;
    }

    // Check which signals are ignored on entry
    // Optimization: Only check signals we actually care about, not all 64!
    // FreeBSD sh approach - only check the signals we might trap
    // This eliminates ~55 unnecessary syscalls on startup
    static const int signals_to_check[] = {
        SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGCHLD, SIGTSTP, SIGTTIN, SIGTTOU,
        SIGPIPE, SIGALRM, SIGUSR1, SIGUSR2, 0
    };
    
    for (int i = 0; signals_to_check[i] != 0; i++) {
        int signum = signals_to_check[i];
        if (signum > 0 && signum < MAX_SIGNALS) {
            sigaction(signum, NULL, &sa);
            if (sa.sa_handler == SIG_IGN) {
                signals_ignored_on_entry[signum] = 1;
            }
        }
    }
    
    // Install SIGINT handler for interactive mode
    struct sigaction sigint_sa;
    sigint_sa.sa_handler = handler;
    sigemptyset(&sigint_sa.sa_mask);
    sigint_sa.sa_flags = 0;  // No SA_RESTART - we want to interrupt syscalls
    sigaction(SIGINT, &sigint_sa, NULL);
}

int signal_get_number(const char *name) {
    if (!name) return -1;
    
    // Check if it's a number
    char *end;
    long val = strtol(name, &end, 10);
    if (*end == '\0') {
        return (int)val;
    }

    // Skip SIG prefix if present
    const char *search_name = name;
    if (strncasecmp(name, "SIG", 3) == 0) {
        search_name += 3;
    }

    for (int i = 0; signal_map[i].name; i++) {
        if (strcasecmp(search_name, signal_map[i].name) == 0) {
            return signal_map[i].number;
        }
    }
    return -1;
}

const char *signal_get_name(int signum) {
    if (signum == 0) return "EXIT";
    for (int i = 0; signal_map[i].name; i++) {
        if (signal_map[i].number == signum) {
            return signal_map[i].name;
        }
    }
    return NULL;
}

int signal_trap(int signum, const char *command) {
    if (signum < 0 || signum >= MAX_SIGNALS) return -1;
    
    // POSIX: Signals ignored on entry cannot be trapped in non-interactive shell
    // But we are mostly interactive or simulating it.
    // However, if we are running a script, we should respect this.
    // For now, let's allow it unless we implement strict non-interactive mode checks.
    
    if (trap_commands[signum]) {
        free(trap_commands[signum]);
    }
    
    if (command && *command) {
        trap_commands[signum] = xstrdup(command);
        
        if (signum > 0) {
            struct sigaction sa;
            sa.sa_handler = handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_RESTART;
            sigaction(signum, &sa, NULL);
        }
    } else {
        // Empty command means ignore
        trap_commands[signum] = NULL; // Or empty string?
        // POSIX: "If action is null (""), the shell shall ignore each specified condition"
        // So we should set handler to SIG_IGN.
        if (signum > 0) {
            struct sigaction sa;
            sa.sa_handler = SIG_IGN;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(signum, &sa, NULL);
        }
    }
    return 0;
}

int signal_reset(int signum) {
    if (signum < 0 || signum >= MAX_SIGNALS) return -1;

    if (trap_commands[signum]) {
        free(trap_commands[signum]);
        trap_commands[signum] = NULL;
    }

    if (signum > 0) {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(signum, &sa, NULL);
    }
    return 0;
}

int signal_ignore(int signum) {
    return signal_trap(signum, "");
}

void signal_list_traps(void) {
    for (int i = 0; i < MAX_SIGNALS; i++) {
        if (trap_commands[i]) {
            const char *name = signal_get_name(i);
            if (name) {
                // Ensure proper quoting for reinput
                printf("trap -- '");
                for (const char *p = trap_commands[i]; *p; p++) {
                    if (*p == '\'') printf("'\\''");
                    else putchar(*p);
                }
                printf("' %s\n", name);
            }
        }
    }
}

void signal_check_pending(void) {
    if (!any_pending_signal) return;
    
    any_pending_signal = 0;

    for (int i = 0; i < MAX_SIGNALS; i++) {
        if (pending_signals[i]) {
            pending_signals[i] = 0;
            if (trap_commands[i]) {
                // Execute trap command
                // We need to parse and execute it.
                // Using a temporary lexer/parser/executor flow.
                // Note: This interrupts current flow.
                // POSIX says: "The action of trap shall be read and executed by the shell when one of the corresponding conditions arises."
                
                // We should probably save/restore exit status?
                // POSIX: "The value of "$?" after the trap action completes shall be the value it had before trap was invoked."
                int saved_status = executor_get_last_status();
                
                Lexer lexer;
                lexer_init(&lexer, trap_commands[i]);
                ASTNode *node = parser_parse(&lexer);
                if (node) {
                    executor_execute(node);
                    ast_free(node);
                }
                
                executor_set_last_status(saved_status);
            }
        }
    }
}

void signal_trigger_exit(void) {
    // Check for other pending signals first (e.g. TERM) which might call exit()
    signal_check_pending();
    
    pending_signals[0] = 1;
    any_pending_signal = 1;
    signal_check_pending();
}

int signal_check_sigint(void) {
    if (got_sigint) {
        got_sigint = 0;
        return 1;
    }
    return 0;
}
