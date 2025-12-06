/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Printf builtin - optimized with buffered output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "builtins.h"
#include "memalloc.h"
#include "buf_output.h"
#include "error.h"

static int has_error = 0;

// Process escape sequences in format string or %b argument
static char *process_escapes(const char *str, int is_b_format, int *stop) {
    size_t len = strlen(str);
    char *result = xmalloc(len + 1);
    size_t out = 0;
    
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\\' && i + 1 < len) {
            i++;
            switch (str[i]) {
                case 'a': result[out++] = '\a'; break;
                case 'b': result[out++] = '\b'; break;
                case 'f': result[out++] = '\f'; break;
                case 'n': result[out++] = '\n'; break;
                case 'r': result[out++] = '\r'; break;
                case 't': result[out++] = '\t'; break;
                case 'v': result[out++] = '\v'; break;
                case '\\': result[out++] = '\\'; break;
                case 'c':
                    if (is_b_format) {
                        *stop = 1;
                        result[out] = '\0';
                        return result;
                    }
                    result[out++] = 'c';
                    break;
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7': {
                    int val = str[i] - '0';
                    if (i + 1 < len && str[i + 1] >= '0' && str[i + 1] <= '7') {
                        i++;
                        val = val * 8 + (str[i] - '0');
                        if (i + 1 < len && str[i + 1] >= '0' && str[i + 1] <= '7') {
                            i++;
                            val = val * 8 + (str[i] - '0');
                        }
                    }
                    result[out++] = (char)val;
                    break;
                }
                default:
                    // Unknown escape - copy backslash and char
                    result[out++] = '\\';
                    result[out++] = str[i];
                    break;
            }
        } else {
            result[out++] = str[i];
        }
    }
    result[out] = '\0';
    return result;
}

// Parse integer argument (handles ', ", and C integer constants)
static long parse_int_arg(const char *arg) {
    if (!arg || !*arg) {
        return 0;
    }
    
    // Handle character constants: 'c or "c
    if ((arg[0] == '\'' || arg[0] == '"') && arg[1]) {
        return (unsigned char)arg[1];
    }
    
    // Parse as C integer constant
    char *endptr;
    errno = 0;
    long val = strtol(arg, &endptr, 0);
    
    if (errno == ERANGE) {
        error_msg("printf: \"%s\" arithmetic overflow", arg);
        has_error = 1;
        return (errno == ERANGE && val < 0) ? LONG_MIN : LONG_MAX;
    }
    
    if (*endptr != '\0') {
        error_msg("printf: \"%s\" not completely converted", arg);
        has_error = 1;
    }
    
    if (*arg == '\0' || (*arg != '-' && *arg != '+' && !isdigit((unsigned char)*arg) 
        && *arg != '\'' && *arg != '"' && *arg != '0')) {
        error_msg("printf: \"%s\" expected numeric value", arg);
        has_error = 1;
    }
    
    return val;
}

int builtin_printf(char **argv) {
    if (!argv[1]) {
        error_msg("printf: missing format string");
        return 1;
    }
    
    // Initialize buffer if needed (though main should have done it)
    // buf_out_init(); // Safe to call multiple times? No, it mallocs.
    // Assume initialized.
    
    char *format = argv[1];
    int arg_idx = 2;
    int len = strlen(format);
    int stop = 0;
    has_error = 0;
    
    // Loop until all arguments are consumed or format string is exhausted
    do {
        int fmt_idx = 0;
        int had_conversion = 0;
        
        while (fmt_idx < len && !stop) {
            if (format[fmt_idx] == '\\') {
                fmt_idx++;
                if (fmt_idx >= len) {
                    OUT_PUTC('\\');
                    break;
                }
                
                switch (format[fmt_idx]) {
                    case 'a': OUT_PUTC('\a'); break;
                    case 'b': OUT_PUTC('\b'); break;
                    case 'f': OUT_PUTC('\f'); break;
                    case 'n': OUT_PUTC('\n'); break;
                    case 'r': OUT_PUTC('\r'); break;
                    case 't': OUT_PUTC('\t'); break;
                    case 'v': OUT_PUTC('\v'); break;
                    case '\\': OUT_PUTC('\\'); break;
                    case 'c': 
                        // Stop output immediately
                        return 0;
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7': {
                        int val = format[fmt_idx] - '0';
                        if (fmt_idx + 1 < len && format[fmt_idx + 1] >= '0' && format[fmt_idx + 1] <= '7') {
                            fmt_idx++;
                            val = val * 8 + (format[fmt_idx] - '0');
                            if (fmt_idx + 1 < len && format[fmt_idx + 1] >= '0' && format[fmt_idx + 1] <= '7') {
                                fmt_idx++;
                                val = val * 8 + (format[fmt_idx] - '0');
                            }
                        }
                        OUT_PUTC((char)val);
                        break;
                    }
                    default:
                        OUT_PUTC('\\');
                        OUT_PUTC(format[fmt_idx]);
                        break;
                }
                fmt_idx++;
            } else if (format[fmt_idx] == '%') {
                if (fmt_idx + 1 < len && format[fmt_idx + 1] == '%') {
                    OUT_PUTC('%');
                    fmt_idx += 2;
                    continue;
                }
                
                had_conversion = 1;
                
                // Parse format specifier
                int start = fmt_idx;
                fmt_idx++;
                while (fmt_idx < len && strchr("-+ #0", format[fmt_idx])) fmt_idx++; // Flags
                while (fmt_idx < len && isdigit(format[fmt_idx])) fmt_idx++; // Width
                if (fmt_idx < len && format[fmt_idx] == '.') { // Precision
                    fmt_idx++;
                    while (fmt_idx < len && isdigit(format[fmt_idx])) fmt_idx++;
                }
                // Length modifier (ignored for now, assume standard)
                if (fmt_idx < len && strchr("hlL", format[fmt_idx])) fmt_idx++;
                
                char conv_char = format[fmt_idx];
                fmt_idx++;
                
                // Extract the full conversion specifier
                int spec_len = fmt_idx - start;
                char conv_spec[64];
                if (spec_len >= 63) spec_len = 63;
                strncpy(conv_spec, format + start, spec_len);
                conv_spec[spec_len] = '\0';
                
                char *arg = argv[arg_idx] ? argv[arg_idx++] : NULL;
                
                if (conv_char == 'b') {
                    if (arg) {
                        char *processed = process_escapes(arg, 1, &stop);
                        OUT_PRINTF("%s", processed);
                        free(processed);
                    }
                } else if (conv_char == 'c') {
                    if (arg) OUT_PUTC((unsigned char)arg[0]);
                } else if (strchr("diouxX", conv_char)) {
                    // Fix up specifier for long
                    char long_spec[70];
                    int p = 0;
                    long_spec[p++] = '%';
                    int s = start + 1;
                    while (s < fmt_idx - 1 && p < 64) long_spec[p++] = format[s++];
                    long_spec[p++] = 'l';
                    long_spec[p++] = conv_char;
                    long_spec[p] = '\0';
                    
                    long val = parse_int_arg(arg);
                    if (strchr("ouxX", conv_char)) {
                        OUT_PRINTF(long_spec, (unsigned long)val);
                    } else {
                        OUT_PRINTF(long_spec, val);
                    }
                } else if (conv_char == 's') {
                    OUT_PRINTF(conv_spec, arg ? arg : "");
                } else {
                    // Unknown specifier, print as is
                    OUT_PUTC('%');
                    if (start + 1 < len) OUT_PUTC(format[start + 1]);
                }
            } else {
                OUT_PUTC(format[fmt_idx++]);
            }
        }
        
        if (!argv[arg_idx] || !had_conversion) {
            break;
        }
    } while (argv[arg_idx] != NULL);

    return has_error ? 1 : 0;
}
