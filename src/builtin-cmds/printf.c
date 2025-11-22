/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "builtins.h"
#include "memalloc.h"

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
        fprintf(stderr, "printf: \"%s\" arithmetic overflow\n", arg);
        has_error = 1;
        return (errno == ERANGE && val < 0) ? LONG_MIN : LONG_MAX;
    }
    
    if (*endptr != '\0') {
        fprintf(stderr, "printf: \"%s\" not completely converted\n", arg);
        has_error = 1;
    }
    
    if (*arg == '\0' || (*arg != '-' && *arg != '+' && !isdigit((unsigned char)*arg) 
        && *arg != '\'' && *arg != '"' && *arg != '0')) {
        fprintf(stderr, "printf: \"%s\" expected numeric value\n", arg);
        has_error = 1;
        return 0;
    }
    
    return val;
}

int builtin_printf(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "printf: missing format string\n");
        return 1;
    }
    
    const char *format = argv[1];
    int arg_idx = 2;
    int stop = 0;
    has_error = 0;
    
    // Process format string, reusing as needed
    do {
        size_t fmt_idx = 0;
        int had_conversion = 0;
        
        while (format[fmt_idx] && !stop) {
            if (format[fmt_idx] == '\\' && fmt_idx + 1 < strlen(format)) {
                // Process escape in format string
                fmt_idx++;
                switch (format[fmt_idx]) {
                    case 'a': putchar('\a'); break;
                    case 'b': putchar('\b'); break;
                    case 'f': putchar('\f'); break;
                    case 'n': putchar('\n'); break;
                    case 'r': putchar('\r'); break;
                    case 't': putchar('\t'); break;
                    case 'v': putchar('\v'); break;
                    case '\\': putchar('\\'); break;
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7': {
                        int val = format[fmt_idx] - '0';
                        if (fmt_idx + 1 < strlen(format) && format[fmt_idx + 1] >= '0' && format[fmt_idx + 1] <= '7') {
                            fmt_idx++;
                            val = val * 8 + (format[fmt_idx] - '0');
                            if (fmt_idx + 1 < strlen(format) && format[fmt_idx + 1] >= '0' && format[fmt_idx + 1] <= '7') {
                                fmt_idx++;
                                val = val * 8 + (format[fmt_idx] - '0');
                            }
                        }
                        putchar((char)val);
                        break;
                    }
                    default:
                        putchar('\\');
                        putchar(format[fmt_idx]);
                        break;
                }
                fmt_idx++;
            } else if (format[fmt_idx] == '%') {
                had_conversion = 1;
                fmt_idx++;
                
                // Parse flags, width, precision
                char conv_spec[100];
                size_t spec_idx = 0;
                conv_spec[spec_idx++] = '%';
                
                // Flags
                while (format[fmt_idx] && strchr("-+ #0", format[fmt_idx])) {
                    conv_spec[spec_idx++] = format[fmt_idx++];
                }
                
                // Width
                while (format[fmt_idx] && isdigit((unsigned char)format[fmt_idx])) {
                    conv_spec[spec_idx++] = format[fmt_idx++];
                }
                
                // Precision
                if (format[fmt_idx] == '.') {
                    conv_spec[spec_idx++] = format[fmt_idx++];
                    while (format[fmt_idx] && isdigit((unsigned char)format[fmt_idx])) {
                        conv_spec[spec_idx++] = format[fmt_idx++];
                    }
                }
                
                // Conversion character
                char conv_char = format[fmt_idx];
                if (conv_char) {
                    fmt_idx++;
                }
                
                const char *arg = argv[arg_idx];
                if (arg) {
                    arg_idx++;
                }
                
                switch (conv_char) {
                    case 'd':
                    case 'i': {
                        conv_spec[spec_idx++] = 'l';
                        conv_spec[spec_idx++] = 'd';
                        conv_spec[spec_idx] = '\0';
                        long val = arg ? parse_int_arg(arg) : 0;
                        printf(conv_spec, val);
                        break;
                    }
                    case 'u': {
                        conv_spec[spec_idx++] = 'l';
                        conv_spec[spec_idx++] = 'u';
                        conv_spec[spec_idx] = '\0';
                        unsigned long val = arg ? (unsigned long)parse_int_arg(arg) : 0;
                        printf(conv_spec, val);
                        break;
                    }
                    case 'o': {
                        conv_spec[spec_idx++] = 'l';
                        conv_spec[spec_idx++] = 'o';
                        conv_spec[spec_idx] = '\0';
                        unsigned long val = arg ? (unsigned long)parse_int_arg(arg) : 0;
                        printf(conv_spec, val);
                        break;
                    }
                    case 'x': {
                        conv_spec[spec_idx++] = 'l';
                        conv_spec[spec_idx++] = 'x';
                        conv_spec[spec_idx] = '\0';
                        unsigned long val = arg ? (unsigned long)parse_int_arg(arg) : 0;
                        printf(conv_spec, val);
                        break;
                    }
                    case 'X': {
                        conv_spec[spec_idx++] = 'l';
                        conv_spec[spec_idx++] = 'X';
                        conv_spec[spec_idx] = '\0';
                        unsigned long val = arg ? (unsigned long)parse_int_arg(arg) : 0;
                        printf(conv_spec, val);
                        break;
                    }
                    case 's': {
                        conv_spec[spec_idx++] = 's';
                        conv_spec[spec_idx] = '\0';
                        printf(conv_spec, arg ? arg : "");
                        break;
                    }
                    case 'c': {
                        if (arg && *arg) {
                            putchar((unsigned char)arg[0]);
                        }
                        break;
                    }
                    case 'b': {
                        // Special POSIX %b - process backslash escapes
                        if (arg) {
                            int b_stop = 0;
                            char *processed = process_escapes(arg, 1, &b_stop);
                            printf("%s", processed);
                            free(processed);
                            if (b_stop) {
                                stop = 1;
                            }
                        }
                        break;
                    }
                    case '%': {
                        putchar('%');
                        arg_idx--; // Don't consume an argument
                        break;
                    }
                    default: {
                        putchar('%');
                        if (conv_char) {
                            putchar(conv_char);
                        }
                        arg_idx--; // Don't consume an argument
                        break;
                    }
                }
            } else {
                putchar(format[fmt_idx++]);
            }
        }
        
        // Continue if we have more arguments and had at least one conversion
        if (!argv[arg_idx] || !had_conversion) {
            break;
        }
    } while (1);
    
    fflush(stdout);
    return has_error ? 1 : 0;
}
