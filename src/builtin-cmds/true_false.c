/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "builtins.h"

int builtin_true(char **argv) {
    (void)argv;
    return 0;
}

int builtin_false(char **argv) {
    (void)argv;
    return 1;
}

int builtin_colon(char **argv) {
    (void)argv;
    return 0;
}
