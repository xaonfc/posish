/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include "output.h"

/* Check if stdout is a TTY */
int output_is_tty(void) {
    return isatty(STDOUT_FILENO);
}

/* Check if stderr is a TTY */
int error_is_tty(void) {
    return isatty(STDERR_FILENO);
}

/* Get the file descriptor for stdout */
int output_get_fd(void) {
    return STDOUT_FILENO;
}

/* Get the file descriptor for stderr */
int error_get_fd(void) {
    return STDERR_FILENO;
}

/* Write to stdout */
ssize_t output_write(const void *buf, size_t count) {
    return write(STDOUT_FILENO, buf, count);
}

/* Write to stderr */
ssize_t error_write(const void *buf, size_t count) {
    return write(STDERR_FILENO, buf, count);
}

/* Formatted output to stdout */
int output_printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vfprintf(stdout, format, ap);
    va_end(ap);
    return result;
}

/* Formatted output to stderr */
int error_printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vfprintf(stderr, format, ap);
    va_end(ap);
    return result;
}

/* Formatted output to stdout with va_list */
int output_vprintf(const char *format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

/* Formatted output to stderr with va_list */
int error_vprintf(const char *format, va_list ap) {
    return vfprintf(stderr, format, ap);
}

/* Flush stdout */
int output_flush(void) {
    return fflush(stdout);
}

/* Flush stderr */
int error_flush(void) {
    return fflush(stderr);
}
