/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "variables.h"

// getopts builtin - parse positional parameters for options
// Usage: getopts optstring name [args...]
#include "memalloc.h"

// Static state for getopts
static int saved_optind = 1;
static int nextchar = 0;
static char *saved_arg_content = NULL;

int builtin_getopts(char **argv) {
    if (!argv[1] || !argv[2]) {
        fprintf(stderr, "getopts: usage: getopts optstring name [args...]\n");
        return 2;
    }
    
    const char *optstring = argv[1];
    const char *varname = argv[2];
    
    // Check for silent error reporting
    int silent_errors = (optstring[0] == ':');
    
    // Get OPTIND (starts at 1)
    char *optind_str = posish_var_get("OPTIND");
    int optind = optind_str ? atoi(optind_str) : 1;
    if (optind_str) free(optind_str);
    
    // Reset state if OPTIND changed unexpectedly
    if (optind != saved_optind) {
        nextchar = 0;
    }
    
    // Determine what we're parsing
    char **parse_argv;
    int parse_argc;
    
    if (argv[3]) {
        // Parse provided args
        parse_argv = &argv[3];
        for (parse_argc = 0; parse_argv[parse_argc]; parse_argc++);
    } else {
        // Parse positional parameters
        parse_argv = posish_var_get_all_positional();
        if (!parse_argv) {
            posish_var_set(varname, "?");
            return 1;
        }
        for (parse_argc = 0; parse_argv[parse_argc]; parse_argc++);
    }
    
    // Check if done
    if (optind > parse_argc) {
        posish_var_set(varname, "?");
        if (!argv[3] && parse_argv) {
            for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
            free(parse_argv);
        }
        saved_optind = optind;
        return 1;
    }
    
    const char *current_arg = parse_argv[optind - 1];
    
    // Check if argument content changed (e.g. set -- changed args but OPTIND stayed same)
    if (saved_arg_content && current_arg && strcmp(current_arg, saved_arg_content) != 0) {
        nextchar = 0;
    }
    
    // Update saved content
    if (saved_arg_content) free(saved_arg_content);
    saved_arg_content = current_arg ? xstrdup(current_arg) : NULL;
    
    // Initialize nextchar if starting new arg
    if (nextchar == 0) {
        // Check for end of options
        if (!current_arg || current_arg[0] != '-' || !current_arg[1]) {
            posish_var_set(varname, "?");
            if (!argv[3] && parse_argv) {
                for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
                free(parse_argv);
            }
            saved_optind = optind;
            return 1;
        }
        
        if (strcmp(current_arg, "--") == 0) {
            // End of options, skip --
            char new_optind[32];
            snprintf(new_optind, sizeof(new_optind), "%d", optind + 1);
            posish_var_set("OPTIND", new_optind);
            saved_optind = optind + 1;
            
            posish_var_set(varname, "?");
            if (!argv[3] && parse_argv) {
                for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
                free(parse_argv);
            }
            return 1;
        }
        
        nextchar = 1; // Skip '-'
    }
    
    // Get option character
    char opt_char = current_arg[nextchar];
    
    // Find in optstring
    const char *opt_pos = strchr(optstring, opt_char);
    
    // Handle invalid option
    if (!opt_pos || opt_char == ':') {
        char opt_str[2] = {opt_char, '\0'};
        posish_var_set("OPTARG", ""); // Unset OPTARG (or set to empty)
        if (silent_errors) {
            posish_var_set(varname, "?");
            posish_var_set("OPTARG", opt_str);
        } else {
            posish_var_set(varname, "?");
            fprintf(stderr, "getopts: illegal option -- %c\n", opt_char);
        }
        
        // Advance
        nextchar++;
        if (current_arg[nextchar] == '\0') {
            nextchar = 0;
            optind++;
        }
        
        char new_optind[32];
        snprintf(new_optind, sizeof(new_optind), "%d", optind);
        posish_var_set("OPTIND", new_optind);
        saved_optind = optind;
        
        if (!argv[3] && parse_argv) {
            for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
            free(parse_argv);
        }
        return 0;
    }
    
    // Valid option
    char opt_str[2] = {opt_char, '\0'};
    posish_var_set(varname, opt_str);
    
    // Check if argument required
    if (opt_pos[1] == ':') {
        if (current_arg[nextchar + 1] != '\0') {
            // Argument is rest of current arg
            posish_var_set("OPTARG", &current_arg[nextchar + 1]);
            optind++;
            nextchar = 0;
        } else if (optind < parse_argc) {
            // Argument is next arg
            posish_var_set("OPTARG", parse_argv[optind]);
            optind += 2;
            nextchar = 0;
        } else {
            // Missing argument
            if (silent_errors) {
                posish_var_set(varname, ":");
                posish_var_set("OPTARG", opt_str);
            } else {
                posish_var_set(varname, "?");
                posish_var_set("OPTARG", "");
                fprintf(stderr, "getopts: option requires an argument -- %c\n", opt_char);
            }
            // Even on error, we consume the option. 
            // POSIX says: "increment OPTIND to the index of the first string after the option-argument"
            // But if missing, we just increment past the option?
            // Usually we just increment OPTIND by 1 (consuming the option flag arg)
            optind++; 
            nextchar = 0;
        }
    } else {
        // No argument required
        posish_var_set("OPTARG", "");
        nextchar++;
        if (current_arg[nextchar] == '\0') {
            optind++;
            nextchar = 0;
        }
    }
    
    // Update OPTIND
    char new_optind[32];
    snprintf(new_optind, sizeof(new_optind), "%d", optind);
    posish_var_set("OPTIND", new_optind);
    saved_optind = optind;
    
    if (!argv[3] && parse_argv) {
        for (int i = 0; parse_argv[i]; i++) free(parse_argv[i]);
        free(parse_argv);
    }
    
    return 0;
}
