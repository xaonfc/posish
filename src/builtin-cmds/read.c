/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "builtins.h"
#include "variables.h"
#include "memalloc.h"

// Helper to check if a char is in IFS
static int is_ifs(char c, const char *ifs) {
    if (!ifs) {
        // Default IFS: space, tab, newline
        return c == ' ' || c == '\t' || c == '\n';
    }
    return strchr(ifs, c) != NULL;
}

// Helper to check if char is IFS whitespace
static int is_ifs_whitespace(char c, const char *ifs) {
    if (!ifs) {
        return c == ' ' || c == '\t' || c == '\n';
    }
    return strchr(ifs, c) && isspace((unsigned char)c);
}

int builtin_read(char **argv) {
    int raw_mode = 0;
    (void)raw_mode; // Suppress unused warning until implemented
    int arg_idx = 1;

    // Parse options
    if (argv[1] && strcmp(argv[1], "-r") == 0) {
        raw_mode = 1;
        arg_idx++;
    }

    // Get variable names
    char **vars = &argv[arg_idx];
    int var_count = 0;
    while (vars[var_count]) var_count++;

    // If no variables provided, use REPLY
    char *default_var[] = {"REPLY", NULL};
    if (var_count == 0) {
        vars = default_var;
        var_count = 1;
    }

    // Read line from stdin with backslash processing
    char *line = NULL;
    size_t capacity = 0;
    size_t len = 0;
    int continuation = 0;

    do {
        char *buf = NULL;
        size_t buf_len = 0;
        ssize_t n = getline(&buf, &buf_len, stdin);
        
        if (n == -1) {
            free(buf);
            if (len == 0) {
                free(line);
                return 1; // EOF or error
            }
            break; // EOF after some content
        }

        // Remove trailing newline
        if (n > 0 && buf[n - 1] == '\n') {
            buf[n - 1] = '\0';
            n--;
        }

        // Check for line continuation if not raw mode
        continuation = 0;
        if (!raw_mode && n > 0 && buf[n - 1] == '\\') {
            continuation = 1;
            buf[n - 1] = '\0'; // Remove backslash
            n--;
        }

        // Append to line
        size_t needed = len + n + 1; // +1 for null terminator
        if (needed > capacity) {
            capacity = needed + 128; // Add extra buffer space
            line = xrealloc(line, capacity);
            if (!line) {
                free(buf);
                return 1;
            }
        }
        
        // Process backslashes in the buffer if not raw mode
        if (!raw_mode) {
            size_t j = 0;
            for (size_t i = 0; i < (size_t)n; i++) {
                if (buf[i] == '\\' && i + 1 < (size_t)n) {
                    // Escape next char
                    line[len + j++] = buf[++i];
                } else {
                    line[len + j++] = buf[i];
                }
            }
            len += j;
        } else {
            if (line && n > 0) {
                memcpy(line + len, buf, n);
            }
            len += n;
        }
        if (line) line[len] = '\0';
        
        free(buf);
        
        // If continuation, prompt if interactive (PS2) - strictly speaking optional for builtin
        // but we should loop to read more
    } while (continuation);

    // Get IFS
    char *ifs_val = posish_var_get("IFS");
    char *ifs = ifs_val;
    
    // Split line into fields
    char *cursor = line;
    
    // Skip leading IFS whitespace
    while (*cursor && is_ifs_whitespace(*cursor, ifs)) {
        cursor++;
    }

    for (int i = 0; i < var_count; i++) {
        char *var_name = vars[i];
        char *value_start = cursor;
        char *value_end = cursor;

        if (i == var_count - 1) {
            // Last variable gets the rest of the line
            // Just trim trailing IFS whitespace
            value_end = cursor + strlen(cursor);
            while (value_end > value_start && is_ifs_whitespace(value_end[-1], ifs)) {
                value_end--;
            }
            // Null terminate
            char saved = *value_end;
            *value_end = '\0';
            posish_var_set(var_name, value_start);
            *value_end = saved; // Restore just in case
            break;
        }

        // Find end of field
        while (*cursor && !is_ifs(*cursor, ifs)) {
            cursor++;
        }
        
        value_end = cursor;
        
        // If we hit a separator, skip it and any adjacent IFS whitespace
        if (*cursor) {
            // If it's IFS whitespace, skip all adjacent IFS whitespace
            if (is_ifs_whitespace(*cursor, ifs)) {
                while (*cursor && is_ifs_whitespace(*cursor, ifs)) {
                    cursor++;
                }
            } else {
                // Non-whitespace IFS char: just skip it, then skip subsequent whitespace
                cursor++;
                while (*cursor && is_ifs_whitespace(*cursor, ifs)) {
                    cursor++;
                }
            }
        }

        // Null terminate and set
        char saved = *value_end;
        *value_end = '\0';
        posish_var_set(var_name, value_start);
        *value_end = saved;
    }

    if (ifs_val) free(ifs_val);
    free(line);
    return 0;
}
