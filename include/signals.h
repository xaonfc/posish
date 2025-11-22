/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

// Initialize signal handling
void signal_init(void);

// Register a trap command for a signal
// Returns 0 on success, -1 on error (e.g. invalid signal)
int signal_trap(int signum, const char *command);

// Reset a signal to its default behavior
int signal_reset(int signum);

// Ignore a signal
int signal_ignore(int signum);

// List all current traps in POSIX format
void signal_list_traps(void);

// Check for pending signals and execute their trap commands
// Should be called frequently (e.g. before prompt, before command execution)
void signal_check_pending(void);

// Trigger an exit trap if one is set for SIGTERM or SIGINT
void signal_trigger_exit(void);

// Get signal number from name (case insensitive, with or without SIG prefix)
// Returns -1 if not found
int signal_get_number(const char *name);

// Get signal name from number
const char *signal_get_name(int signum);

#endif // SIGNALS_H
