/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"
#include "variables.h"
#include "error.h"
#include <stdlib.h>

int builtin_shift(char **args) {
    int n = 1;
    if (args[1]) {
        n = atoi(args[1]);
        if (n < 0) {
            error_msg("shift: shift count must be non-negative");
            return 1;
        }
    }
    
    int count = posish_var_get_positional_count();
    if (n > count) {
        error_msg("shift: shift count must be <= $#");
        return 1;
    }
    
    if (posish_var_shift_positional(n) != 0) {
        error_msg("shift: shift count must be <= $#");
        return 1;
    }
    
    return 0;
    
    return 0;
}
