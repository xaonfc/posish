/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "variables.h"
#include "shell_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int builtin_set(char **args) {
    if (!args[1]) {
        // List all variables
        char **vars = var_get_all();
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
    
    var_set_positional(count, args + start_idx);
    return 0;
}
