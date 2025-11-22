/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "error.h"
#include "variables.h"
#include <string.h> // Required for strcmp
#include <limits.h> // Required for PATH_MAX

int builtin_cd(char **argv) {
    const char *new_dir = argv[1];
    char cwd[PATH_MAX];
    
    // Get current directory before changing
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd[0] = '\0';
    }
    
    // No argument: go to $HOME
    if (!new_dir) {
        new_dir = var_get("HOME");
        if (!new_dir) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    
    // Handle "cd -" (go to OLDPWD)
    if (strcmp(new_dir, "-") == 0) {
        char *oldpwd = var_get("OLDPWD");
        if (!oldpwd || !*oldpwd) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        new_dir = oldpwd;
        printf("%s\n", new_dir); // Print directory when using cd -
    }
    
    if (chdir(new_dir) != 0) {
        perror("cd");
        return 1;
    }
    
    // Set OLDPWD to previous directory
    if (cwd[0] != '\0') {
        var_set("OLDPWD", cwd);
    }
    
    // Update PWD
    if (getcwd(cwd, sizeof(cwd))) {
        var_set("PWD", cwd);
    }
    
    return 0;
}
