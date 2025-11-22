/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <unistd.h>
#include "input.h"

/* Check if stdin is a TTY */
int input_is_tty(void) {
    return isatty(STDIN_FILENO);
}

/* Get the file descriptor for stdin */
int input_get_fd(void) {
    return STDIN_FILENO;
}

/* Read a character from stdin */
int input_read_char(char *c) {
    ssize_t result = read(STDIN_FILENO, c, 1);
    return (result == 1) ? (unsigned char)*c : -1;
}

/* Read from stdin using getline */
ssize_t input_getline(char **lineptr, size_t *n) {
    return getline(lineptr, n, stdin);
}
