/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "variables.h"

// getopts builtin - parse positional parameters for options
// Usage: getopts optstring name [args...]
int builtin_getopts(char **argv) {
    if (!argv[1] || !argv[2]) {
        fprintf(stderr, "getopts: usage: getopts optstring name [args...]\n");
        return 2;
    }
    
    const char *optstring = argv[1];
    const char *varname = argv[2];
    
    // Get OPTIND (starts at 1)
    char *optind_str = var_get("OPTIND");
    int optind = optind_str ? atoi(optind_str) : 1;
    if (optind_str) free(optind_str);
    
    // Determine what we're parsing
    char **parse_argv;
    int parse_argc;
    
    if (argv[3]) {
        // Parse provided args
        parse_argv = &argv[3];
        for (parse_argc = 0; parse_argv[parse_argc]; parse_argc++);
    } else {
        // Parse positional parameters
        parse_argv = var_get_all_positional();
        if (!parse_argv) {
            var_set(varname, "?");
            return 1;
        }
        for (parse_argc = 0; parse_argv[parse_argc]; parse_argc++);
    }
    
    // Check if done
    if (optind > parse_argc) {
        var_set(varname, "?");
        if (!argv[3] && parse_argv) {
            for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
            free(parse_argv);
        }
        return 1;
    }
    
    const char *current_arg = parse_argv[optind - 1];
    
    // Must start with '-'
    if (!current_arg || current_arg[0] != '-' || !current_arg[1] || strcmp(current_arg, "--") == 0) {
        var_set(varname, "?");
        if (!argv[3] && parse_argv) {
            for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
            free(parse_argv);
        }
        return 1;
    }
    
    // Get option character (after '-')
    char opt_char = current_arg[1];
    
    // Find in optstring
    const char *opt_pos = strchr(optstring, opt_char);
    if (!opt_pos) {
        // Invalid option
        fprintf(stderr, "getopts: illegal option -- %c\n", opt_char);
        var_set(varname, "?");
        var_set("OPTARG", "");
        
        // Move to next arg
        char new_optind[32];
        snprintf(new_optind, sizeof(new_optind), "%d", optind + 1);
        var_set("OPTIND", new_optind);
        
        if (!argv[3] && parse_argv) {
            for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
            free(parse_argv);
        }
        return 1;
    }
    
    // Set the option character
    char opt_str[2] = {opt_char, '\0'};
    var_set(varname, opt_str);
    
    // Check if option requires argument (followed by ':')
    if (opt_pos[1] == ':') {
        // Requires argument
        if (current_arg[2]) {
            // Argument is rest of current arg: -oARG
            var_set("OPTARG", &current_arg[2]);
        } else if (optind < parse_argc) {
            // Argument is next arg: -o ARG
            var_set("OPTARG", parse_argv[optind]);
            optind++;
        } else {
            // Missing argument
            fprintf(stderr, "getopts: option requires an argument -- %c\n", opt_char);
            var_set(varname, "?");
            var_set("OPTARG", "");
            
            char new_optind[32];
            snprintf(new_optind, sizeof(new_optind), "%d", optind + 1);
            var_set("OPTIND", new_optind);
            
            if (!argv[3] && parse_argv) {
                for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
                free(parse_argv);
            }
            return 1;
        }
    } else {
        var_set("OPTARG", "");
    }
    
    // Update OPTIND for next call
    char new_optind[32];
    snprintf(new_optind, sizeof(new_optind), "%d", optind + 1);
    var_set("OPTIND", new_optind);
    
    if (!argv[3] && parse_argv) {
        for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
        free(parse_argv);
    }
    
    return 0;
}
