/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include "builtins.h"
#include "error.h"

// Forward declarations
static int eval_expr(char **args, int *pos, int end);
static int eval_or(char **args, int *pos, int end);
static int eval_and(char **args, int *pos, int end);
static int eval_primary(char **args, int *pos, int end);

// Check if string is a unary operator
static int is_unary_op(const char *s) {
    if (!s || s[0] != '-' || !s[1] || s[2]) return 0;
    return strchr("bcdefghLnprSstuwxz", s[1]) != NULL;
}

// Check if string is a binary operator
static int is_binary_op(const char *s) {
    if (!s) return 0;
    if (strcmp(s, "=") == 0 || strcmp(s, "!=") == 0) return 1;
    if (s[0] != '-') return 0;
    if (!strcmp(s, "-eq") || !strcmp(s, "-ne") || !strcmp(s, "-gt") ||
        !strcmp(s, "-ge") || !strcmp(s, "-lt") || !strcmp(s, "-le")) return 1;
    return 0;
}

// File test helpers
static int file_test(const char *path, char op) {
    struct stat st;
    int use_lstat = (op == 'h' || op == 'L');
    int result = use_lstat ? lstat(path, &st) : stat(path, &st);
    
    if (result != 0) return 0; // Cannot stat
    
    switch (op) {
        case 'b': return S_ISBLK(st.st_mode);
        case 'c': return S_ISCHR(st.st_mode);
        case 'd': return S_ISDIR(st.st_mode);
        case 'e': return 1; // Exists
        case 'f': return S_ISREG(st.st_mode);
        case 'g': return (st.st_mode & S_ISGID) != 0;
        case 'h': case 'L': return S_ISLNK(st.st_mode);
        case 'p': return S_ISFIFO(st.st_mode);
        case 'r': return access(path, R_OK) == 0;
        case 'S': return S_ISSOCK(st.st_mode);
        case 's': return st.st_size > 0;
        case 'u': return (st.st_mode & S_ISUID) != 0;
        case 'w': return access(path, W_OK) == 0;
        case 'x': return access(path, X_OK) == 0;
        default: return 0;
    }
}

// Integer comparison
static int int_cmp(const char *s1, const char *op, const char *s2) {
    char *end1, *end2;
    long n1 = strtol(s1, &end1, 10);
    long n2 = strtol(s2, &end2, 10);
    
    if (*end1 != '\0' || *end2 != '\0') {
        error_msg("test: integer expression expected");
        return 0;
    }
    
    if (!strcmp(op, "-eq")) return n1 == n2;
    if (!strcmp(op, "-ne")) return n1 != n2;
    if (!strcmp(op, "-gt")) return n1 > n2;
    if (!strcmp(op, "-ge")) return n1 >= n2;
    if (!strcmp(op, "-lt")) return n1 < n2;
    if (!strcmp(op, "-le")) return n1 <= n2;
    return 0;
}

// Evaluate a primary (unary or binary test)
static int eval_primary(char **args, int *pos, int end) {
    if (*pos >= end) return 0;
    
    // Handle !
    if (strcmp(args[*pos], "!") == 0) {
        (*pos)++;
        return !eval_primary(args, pos, end);
    }
    
    // Handle ( expr )
    if (strcmp(args[*pos], "(") == 0) {
        (*pos)++;
        int result = eval_expr(args, pos, end);
        if (*pos < end && strcmp(args[*pos], ")") == 0) {
            (*pos)++;
        }
        return result;
    }
    
    // Unary operators
    if (*pos + 1 < end && is_unary_op(args[*pos])) {
        char *op = args[*pos];
        char *arg = args[*pos + 1];
        *pos += 2;
        
        if (strcmp(op, "-n") == 0) return strlen(arg) > 0;
        if (strcmp(op, "-z") == 0) return strlen(arg) == 0;
        if (strcmp(op, "-t") == 0) {
            char *endptr;
            int fd = strtol(arg, &endptr, 10);
            if (*endptr != '\0') return 0;
            return isatty(fd);
        }
        
        // File tests
        return file_test(arg, op[1]);
    }
    
    // Binary operators
    if (*pos + 2 < end && is_binary_op(args[*pos + 1])) {
        char *arg1 = args[*pos];
        char *op = args[*pos + 1];
        char *arg2 = args[*pos + 2];
        *pos += 3;
        
        if (strcmp(op, "=") == 0) return strcmp(arg1, arg2) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(arg1, arg2) != 0;
        
        return int_cmp(arg1, op, arg2);
    }
    
    // Single string (non-null test)
    if (*pos < end) {
        char *str = args[*pos];
        (*pos)++;
        return strlen(str) > 0;
    }
    
    return 0;
}

// Evaluate AND expression (higher precedence than OR)
static int eval_and(char **args, int *pos, int end) {
    int result = eval_primary(args, pos, end);
    
    while (*pos < end && strcmp(args[*pos], "-a") == 0) {
        (*pos)++;
        int right = eval_primary(args, pos, end);
        result = result && right;
    }
    
    return result;
}

// Evaluate OR expression
static int eval_or(char **args, int *pos, int end) {
    int result = eval_and(args, pos, end);
    
    while (*pos < end && strcmp(args[*pos], "-o") == 0) {
        (*pos)++;
        int right = eval_and(args, pos, end);
        result = result || right;
    }
    
    return result;
}

// Top-level expression evaluator
static int eval_expr(char **args, int *pos, int end) {
    return eval_or(args, pos, end);
}

int builtin_test(char **args) {
    int argc = 0;
    while (args[argc]) argc++;
    
    int is_bracket = (strcmp(args[0], "[") == 0);
    
    // Handle [ ... ] syntax
    if (is_bracket) {
        if (argc < 2 || strcmp(args[argc-1], "]") != 0) {
            error_msg("[: missing ]");
            return 2;
        }
        argc--; // Ignore trailing ]
    }
    
    // Skip command name
    char **argv = args + 1;
    int count = argc - 1;
    
    // POSIX argument count rules
    if (count == 0) return 1; // False
    
    if (count == 1) {
        // Single string test
        return (strlen(argv[0]) > 0) ? 0 : 1;
    }
    
    if (count == 2) {
        // ! expr or unary op
        if (strcmp(argv[0], "!") == 0) {
            return (strlen(argv[1]) > 0) ? 1 : 0;
        }
        if (is_unary_op(argv[0])) {
            int pos = 0;
            int result = eval_primary(argv, &pos, count);
            return result ? 0 : 1;
        }
        // Otherwise unspecified, treat as string
        return (strlen(argv[0]) > 0) ? 0 : 1;
    }
    
    if (count == 3) {
        // Binary test or ! unary or ( expr )
        if (is_binary_op(argv[1])) {
            int pos = 0;
            int result = eval_primary(argv, &pos, count);
            return result ? 0 : 1;
        }
        if (strcmp(argv[0], "!") == 0) {
            // ! followed by 2-arg test
            int pos = 1;
            int result = eval_primary(argv, &pos, count);
            return result ? 1 : 0;
        }
        if (strcmp(argv[0], "(") == 0 && strcmp(argv[2], ")") == 0) {
            // ( string )
            return (strlen(argv[1]) > 0) ? 0 : 1;
        }
        // Otherwise unspecified
        return 2;
    }
    
    if (count == 4) {
        // ! 3-arg or ( 2-arg )
        if (strcmp(argv[0], "!") == 0) {
            int pos = 1;
            int result = eval_primary(argv, &pos, count);
            return result ? 1 : 0;
        }
        if (strcmp(argv[0], "(") == 0 && strcmp(argv[3], ")") == 0) {
            int pos = 1;
            int result = eval_primary(argv, &pos, 3);
            return result ? 0 : 1;
        }
        // Otherwise unspecified
        return 2;
    }
    
    // >4 arguments: Use full expression parser
    int pos = 0;
    int result = eval_expr(argv, &pos, count);
    return result ? 0 : 1;
}
