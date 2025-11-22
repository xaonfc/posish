/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>

/* Check if stdin is a TTY */
int input_is_tty(void);

/* Get the file descriptor for stdin */
int input_get_fd(void);

/* Read a character from stdin */
int input_read_char(char *c);

/* Read from stdin using getline */
ssize_t input_getline(char **lineptr, size_t *n);

#endif /* INPUT_H */
