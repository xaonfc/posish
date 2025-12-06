/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Buffered output system for high-performance I/O
 * Design inspired by FreeBSD sh architecture
 */

#ifndef BUF_OUTPUT_H
#define BUF_OUTPUT_H

#include <unistd.h>
#include <stddef.h>

struct buf_out {
    char *next;     // Next write position
    char *end;      // Buffer end
    char *start;    // Buffer start
    int fd;         // File descriptor
    size_t size;    // Buffer size
};

// Global stdout buffer
extern struct buf_out buf_stdout;

// Initialize buffered output system
void buf_out_init(void);

// Flush a specific buffer
void buf_out_flush(struct buf_out *buf);

// Flush all registered buffers (e.g. stdout)
void buf_out_flush_all(void);

// Reset a specific buffer (clear content without flushing)
// Useful for child processes after fork to avoid duplicate output
void buf_out_reset(struct buf_out *buf);

// Reset all registered buffers
void buf_out_reset_all(void);

// Slow path for character output (when buffer is full)
void buf_out_putc_slow(int c, struct buf_out *buf);

// Write string to buffer
void buf_out_puts(const char *str, struct buf_out *buf);

// Formatted output to buffer
void buf_out_printf(struct buf_out *buf, const char *fmt, ...);

// Fast inline write macro
#define BUF_PUTC(c, buf) \
    do { \
        if ((buf)->next < (buf)->end) { \
            *(buf)->next++ = (c); \
        } else { \
            buf_out_putc_slow((c), (buf)); \
        } \
    } while (0)

// Convenience macros for stdout
#define OUT_PUTC(c) BUF_PUTC(c, &buf_stdout)
#define OUT_PUTS(s) buf_out_puts(s, &buf_stdout)
#define OUT_PRINTF(...) buf_out_printf(&buf_stdout, __VA_ARGS__)
#define OUT_FLUSH() buf_out_flush(&buf_stdout)

#endif // BUF_OUTPUT_H
