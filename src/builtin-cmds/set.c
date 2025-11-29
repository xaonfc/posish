/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "variables.h"
#include "shell_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Option name to flag mapping for -o support
static const struct {
    const char *name;
    int *flag_ptr;
    char short_opt;  // '\0' if no single-letter equivalent
} option_map[] = {
    {"allexport", &shell_all_export, 'a'},
    {"errexit", &shell_exit_on_error, 'e'},
    {"ignoreeof", &shell_ignore_eof, '\0'},
    {"monitor", &shell_monitor, 'm'},
    {"noclobber", &shell_no_clobber, 'C'},
    {"noglob", &shell_no_glob, 'f'},
    {"noexec", &shell_no_exec, 'n'},
    {"nolog", &shell_nolog, '\0'},
    {"notify", &shell_notify, 'b'},
    {"nounset", &shell_no_unset, 'u'},
    {"verbose", &shell_verbose, 'v'},
    {"vi", &shell_vi_mode, '\0'},
    {"xtrace", &shell_trace_mode, 'x'},
    {NULL, NULL, '\0'}
};

// Print option settings
static void print_options(int reinput_format) {
    if (reinput_format) {
        // +o format: suitable for reinput to shell
        for (int i = 0; option_map[i].name; i++) {
            const char *state = *option_map[i].flag_ptr ? "-o" : "+o";
            printf("set %s %s\n", state, option_map[i].name);
        }
    } else {
        // -o format: human-readable
        for (int i = 0; option_map[i].name; i++) {
            const char *state = *option_map[i].flag_ptr ? "on" : "off";
            printf("%-12s\t%s\n", option_map[i].name, state);
        }
    }
}

// Set or unset a named option
static int set_named_option(const char *name, int enable) {
    for (int i = 0; option_map[i].name; i++) {
        if (strcmp(name, option_map[i].name) == 0) {
            *option_map[i].flag_ptr = enable;
            return 0;
        }
    }
    fprintf(stderr, "set: %s: invalid option name\n", name);
    return 1;
}

int builtin_set(char **args) {
    if (!args[1]) {
        // List all variables
        char **vars = posish_var_get_all();
        for (char **v = vars; *v != NULL; v++) {
            printf("%s\n", *v);
            free(*v);
        }
        free(vars);
        return 0;
    }

    // Handle options
    int start_idx = 1;
    
    while (args[start_idx] && args[start_idx][0] == '-' && args[start_idx][1] != '\0') {
        if (strcmp(args[start_idx], "--") == 0) {
            start_idx++;
            break;
        }
        
        // Handle -o option
        if (strcmp(args[start_idx], "-o") == 0) {
            if (!args[start_idx + 1]) {
                // No argument: print current settings
                print_options(0);
                return 0;
            }
            // Set named option
            start_idx++;
            if (set_named_option(args[start_idx], 1) != 0) {
                return 1;
            }
            start_idx++;
            continue;
        }
        
        const char *opts = &args[start_idx][1];
        int enable = (args[start_idx][0] == '-');
        
        for (const char *p = opts; *p; p++) {
            switch (*p) {
                case 'x': shell_trace_mode = enable; break;
                case 'e': shell_exit_on_error = enable; break;
                case 'f': shell_no_glob = enable; break;
                case 'C': shell_no_clobber = enable; break;
                case 'u': shell_no_unset = enable; break;
                case 'v': shell_verbose = enable; break;
                case 'n': shell_no_exec = enable; break;
                case 'a': shell_all_export = enable; break;
                case 'm': shell_monitor = enable; break;
                case 'h': shell_hash_all = enable; break;
                case 'b': shell_notify = enable; break;
                default:
                    fprintf(stderr, "set: -%c: invalid option\n", *p);
                    return 1;
            }
        }
        start_idx++;
    }
    
    // Handle +x format (disable options)
    int i = 1;
    while (args[i] && args[i][0] == '+' && args[i][1] != '\0') {
        // Handle +o option
        if (strcmp(args[i], "+o") == 0) {
            if (!args[i + 1]) {
                // No argument: print in reinput format
                print_options(1);
                return 0;
            }
            // Unset named option
            i++;
            if (set_named_option(args[i], 0) != 0) {
                return 1;
            }
            start_idx = ++i;
            continue;
        }
        
        const char *opts = &args[i][1];
        for (const char *p = opts; *p; p++) {
            switch (*p) {
                case 'x': shell_trace_mode = 0; break;
                case 'e': shell_exit_on_error = 0; break;
                case 'f': shell_no_glob = 0; break;
                case 'C': shell_no_clobber = 0; break;
                case 'u': shell_no_unset = 0; break;
                case 'v': shell_verbose = 0; break;
                case 'n': shell_no_exec = 0; break;
                case 'a': shell_all_export = 0; break;
                case 'm': shell_monitor = 0; break;
                case 'h': shell_hash_all = 0; break;
                case 'b': shell_notify = 0; break;
                default:
                    fprintf(stderr, "set: +%c: invalid option\n", *p);
                    return 1;
            }
        }
        start_idx = ++i;
    }
    
    // If no more args, we're done
    if (!args[start_idx]) {
        return 0;
    }

    // Count remaining args for positional parameters
    int count = 0;
    for (int j = start_idx; args[j] != NULL; j++) {
        count++;
    }
    
    posish_var_set_positional(count, args + start_idx);
    return 0;
}
