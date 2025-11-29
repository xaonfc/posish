/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "error.h"
#include "output.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "buf_output.h"

#include "variables.h"

static void print_prefix(void) {
    char *name = posish_var_get_shell_name();
    if (name) {
        error_printf("%s: ", name);
        free(name);
    } else {
        error_printf("posish: ");
    }
}

void error_msg(const char *fmt, ...) {
    print_prefix();
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
}

void error_sys(const char *fmt, ...) {
    print_prefix();
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf(": %s\n", strerror(errno));
}

void error_fatal(const char *fmt, ...) {
    print_prefix();
    va_list ap;
    va_start(ap, fmt);
    error_vprintf(fmt, ap);
    va_end(ap);
    error_printf("\n");
    buf_out_flush_all();
    exit(1);
}
