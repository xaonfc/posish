/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef BLTIN_H
#define BLTIN_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "output.h"
#include "error.h"
#include "memalloc.h"

/* 
 * Compatibility layer for FreeBSD builtins (like test.c)
 * This maps the internal API expected by FreeBSD sh builtins 
 * to posish's infrastructure.
 */

/* Output mapping */
#undef stdout
#define stdout ((void*)1) /* Dummy pointer, not used but needed for macros */
#undef stderr
#define stderr ((void*)2)

/* We map standard I/O calls to our output.c functions */
#define out1 output_printf
#define out2 error_printf
#define outfmt(file, fmt, ...) ((file) == stdout ? output_printf(fmt, ##__VA_ARGS__) : error_printf(fmt, ##__VA_ARGS__))

/* printf macros used by FreeBSD sh */
#undef printf
#define printf output_printf
#undef fprintf
#define fprintf(file, fmt, ...) outfmt(file, fmt, ##__VA_ARGS__)

/* putc/putchar macros */
#undef putc
#define putc(c, file) ((file) == stdout ? output_printf("%c", c) : error_printf("%c", c))
#undef putchar
#define putchar(c) output_printf("%c", c)

/* Error handling mapping */
/* FreeBSD sh uses error() for fatal errors (longjmp) and warn() for warnings */
/* Error handling mapping */
/* FreeBSD sh uses error() for fatal errors (longjmp) and warn() for warnings */
static inline void bltin_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
    exit(2); /* FreeBSD test exits with 2 on error */
}

static inline void bltin_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
}

static inline void bltin_warnx(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
}

static inline void bltin_errx(int status, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
    exit(status);
}

#define error bltin_error
#define warn bltin_warn
#define warnx bltin_warnx
#define errx bltin_errx

/* Memory allocation */
#define stalloc xmalloc
#define ckmalloc xmalloc
#define ckfree free

/* Types */
typedef void *pointer;

/* Misc */
#define INITARGS(argv)
extern char *commandname;

/* Map eaccess to access (POSIX) - strictly speaking eaccess checks effective UID, 
   but for now access is a reasonable fallback or we can use faccessat with AT_EACCESS if available */
#define eaccess access

#endif /* BLTIN_H */
