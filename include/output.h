/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>
#include <stdarg.h>

/* Check if stdout is a TTY */
int output_is_tty(void);

/* Check if stderr is a TTY */
int error_is_tty(void);

/* Get the file descriptor for stdout */
int output_get_fd(void);

/* Get the file descriptor for stderr */
int error_get_fd(void);

/* Write to stdout */
ssize_t output_write(const void *buf, size_t count);

/* Write to stderr */
ssize_t error_write(const void *buf, size_t count);

/* Formatted output to stdout */
int output_printf(const char *format, ...);

/* Formatted output to stderr */
int error_printf(const char *format, ...);

/* Formatted output to stdout with va_list */
int output_vprintf(const char *format, va_list ap);

/* Formatted output to stderr with va_list */
int error_vprintf(const char *format, va_list ap);

/* Flush stdout */
int output_flush(void);

/* Flush stderr */
int error_flush(void);

#endif /* OUTPUT_H */
