/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "builtins.h"

// Convert mode to symbolic string (e.g., "u=rwx,g=rx,o=rx")
static void mode_to_symbolic(mode_t mask, char *buf, size_t bufsize) {
    mode_t perms = 0777 & ~mask; // Get actual permissions (inverse of mask)
    
    snprintf(buf, bufsize, "u=%s%s%s,g=%s%s%s,o=%s%s%s",
        (perms & S_IRUSR) ? "r" : "",
        (perms & S_IWUSR) ? "w" : "",
        (perms & S_IXUSR) ? "x" : "",
        (perms & S_IRGRP) ? "r" : "",
        (perms & S_IWGRP) ? "w" : "",
        (perms & S_IXGRP) ? "x" : "",
        (perms & S_IROTH) ? "r" : "",
        (perms & S_IWOTH) ? "w" : "",
        (perms & S_IXOTH) ? "x" : "");
}

int builtin_umask(char **argv) {
    int symbolic = 0;
    int arg_idx = 1;
    
    // Check for -S option
    if (argv[1] && strcmp(argv[1], "-S") == 0) {
        symbolic = 1;
        arg_idx = 2;
    }
    
    // No mask argument - display current mask
    if (!argv[arg_idx]) {
        mode_t current_mask = umask(0);
        umask(current_mask); // Restore it
        
        if (symbolic) {
            char buf[64];
            mode_to_symbolic(current_mask, buf, sizeof(buf));
            printf("%s\n", buf);
        } else {
            printf("%04o\n", current_mask);
        }
        return 0;
    }
    
    // Set new mask
    const char *mask_str = argv[arg_idx];
    mode_t new_mask;
    
    // Parse octal mask
    char *endptr;
    long mask_val = strtol(mask_str, &endptr, 8);
    
    if (*endptr == '\0' && mask_val >= 0 && mask_val <= 0777) {
        // Valid octal
        new_mask = (mode_t)mask_val;
        umask(new_mask);
        return 0;
    }
    
    
    // Parse symbolic mask (u=rwx,g=rx,o=rx format)
    mode_t symbolic_mask = 0;
    const char *p = mask_str;
    int valid_symbolic = 1;
    
    while (*p && valid_symbolic) {
        // Parse who: u, g, o, or a (all)
        mode_t who_mask = 0;
        int has_who = 0;
        
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            has_who = 1;
            if (*p == 'u') who_mask |= 0700;
            else if (*p == 'g') who_mask |= 0070;
            else if (*p == 'o') who_mask |= 0007;
            else if (*p == 'a') who_mask = 0777;
            p++;
        }
        
        if (!has_who) who_mask = 0777; // Default to all
        
        // Parse op: =, +, or -
        char op = *p;
        if (op != '=' && op != '+' && op != '-') {
            valid_symbolic = 0;
            break;
        }
        p++;
        
        // Parse perms: r, w, x
        mode_t perm_bits = 0;
        while (*p == 'r' || *p == 'w' || *p == 'x') {
            if (*p == 'r') perm_bits |= 0444;
            else if (*p == 'w') perm_bits |= 0222;
            else if (*p == 'x') perm_bits |= 0111;
            p++;
        }
        
        // Apply to mask based on operation
        perm_bits &= who_mask; // Only apply to specified who
        
        if (op == '=') {
            // Set: clear existing bits for who, then remove perm_bits from mask
            symbolic_mask = (symbolic_mask & ~who_mask) | (who_mask & ~perm_bits);
        } else if (op == '+') {
            // Add: remove perm_bits from mask (granting permission)
            symbolic_mask &= ~perm_bits;
        } else if (op == '-') {
            // Remove: add perm_bits to mask (denying permission)
            symbolic_mask |= perm_bits;
        }
        
        // Skip comma if present
        if (*p == ',') p++;
    }
    
    if (valid_symbolic && *p == '\0') {
        umask(symbolic_mask);
        return 0;
    }
    
    fprintf(stderr, "umask: invalid mask: %s\n", mask_str);
    return 1;
}
