/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Buffered output system implementation
 */

#include "buf_output.h"
#include "memalloc.h"
#include "signals.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define BUF_SIZE 8192

struct buf_out buf_stdout;

void buf_out_init(void) {
    // Initialize stdout buffer
    buf_stdout.size = BUF_SIZE;
    buf_stdout.start = xmalloc(BUF_SIZE);
    buf_stdout.next = buf_stdout.start;
    buf_stdout.end = buf_stdout.start + BUF_SIZE;
    buf_stdout.fd = STDOUT_FILENO;
}

void buf_out_flush(struct buf_out *buf) {
    if (buf->start == NULL || buf->next == buf->start || buf->fd < 0) {
        return;
    }
    
    size_t len = buf->next - buf->start;
    size_t written = 0;
    
    /* Write with EINTR retry */
    while (written < len) {
        ssize_t n = write(buf->fd, buf->start + written, len - written);
        if (n < 0) {
            if (errno == EINTR) {
                if (got_sigint) {
                    // Stop writing if SIGINT received
                    buf->next = buf->start; // Discard buffer
                    return;
                }
                continue;
            }
            // If EPIPE (broken pipe), we might want to exit or stop writing?
            // For now, just stop trying to write this chunk.
            break; 
        }
        written += n;
    }
    
    buf->next = buf->start;
}

void buf_out_flush_all(void) {
    buf_out_flush(&buf_stdout);
}

void buf_out_reset(struct buf_out *buf) {
    if (buf->start) {
        buf->next = buf->start;
    }
}

void buf_out_reset_all(void) {
    buf_out_reset(&buf_stdout);
}

void buf_out_putc_slow(int c, struct buf_out *buf) {
    buf_out_flush(buf);
    *(buf)->next++ = c;
}

void buf_out_puts(const char *str, struct buf_out *buf) {
    while (*str) {
        BUF_PUTC(*str++, buf);
    }
}

void buf_out_printf(struct buf_out *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[4096]; // Large enough for most shell outputs
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    
    if (len > 0) {
        buf_out_puts(tmp, buf);
    }
}
