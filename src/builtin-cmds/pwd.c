/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "error.h"
#include "variables.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

int builtin_pwd(char **args) {
    int logical = 1; // Default is logical
    
    // Parse options
    int i = 1;
    while (args[i] && args[i][0] == '-') {
        if (strcmp(args[i], "--") == 0) {
            i++;
            break;
        }
        for (int j = 1; args[i][j]; j++) {
            if (args[i][j] == 'L') {
                logical = 1;
            } else if (args[i][j] == 'P') {
                logical = 0;
            } else {
                error_msg("pwd: -%c: invalid option", args[i][j]);
                return 1;
            }
        }
        i++;
    }
    
    if (logical) {
        char *pwd = posish_var_get("PWD");
        if (pwd) {
            printf("%s\n", pwd);
            free(pwd);
            return 0;
        }
    }
    
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;
    } else {
        error_sys("pwd");
        return 1;
    }
}
