/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtins.h"
#include "variables.h"

// Simple implementation of command builtin
// POSIX: Execute command bypassing function lookup
int builtin_command(char **argv) {
    int verbose = 0;
    int very_verbose = 0;
    int use_default_path = 0;
    int arg_idx = 1;
    
    // Parse options
    while (argv[arg_idx] && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "-v") == 0) {
            verbose = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-V") == 0) {
            very_verbose = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-p") == 0) {
            use_default_path = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--") == 0) {
            arg_idx++;
            break;
        } else {
            break;
        }
    }
    
    if (!argv[arg_idx]) {
        fprintf(stderr, "command: missing command name\n");
        return 1;
    }
    
    const char *cmd_name = argv[arg_idx];
    
    // -v: Print pathname of command
    if (verbose || very_verbose) {
        // Check if builtin
        if (builtin_is_builtin(cmd_name)) {
            if (very_verbose) {
                printf("%s is a shell builtin\n", cmd_name);
            } else {
                printf("%s\n", cmd_name);
            }
            return 0;
        }
        
        // Search in PATH
        char *path = use_default_path ? "/usr/bin:/bin" : var_get("PATH");
        if (!path) {
            return 1;
        }
        
        char *path_copy = strdup(path);
        char *dir = strtok(path_copy, ":");
        
        while (dir) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd_name);
            
            if (access(full_path, X_OK) == 0) {
                if (very_verbose) {
                    printf("%s is %s\n", cmd_name, full_path);
                } else {
                    printf("%s\n", full_path);
                }
                free(path_copy);
                if (!use_default_path) free(path);
                return 0;
            }
            
            dir = strtok(NULL, ":");
        }
        
        free(path_copy);
        if (!use_default_path && path) free(path);
        return 1;
    }
    
    // Execute command (bypassing functions)
    // For now, just note that executor needs to skip function lookup
    // This would require modifying executor.c to check for COMMAND_BYPASS flag
    fprintf(stderr, "command: execution mode not fully implemented\n");
    return 1;
}
