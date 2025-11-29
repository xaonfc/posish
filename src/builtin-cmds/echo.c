#include "builtins.h"
#include "buf_output.h"
#include <string.h>

int builtin_echo(char **args) {
    int suppress_newline = 0;
    int start_idx = 1;
    
    // Check for -n option
    if (args[1] && strcmp(args[1], "-n") == 0) {
        suppress_newline = 1;
        start_idx = 2;
    }
    
    for (int i = start_idx; args[i] != NULL; i++) {
        if (i > start_idx) {
            OUT_PUTC(' ');
        }
        OUT_PUTS(args[i]);
    }
    
    if (!suppress_newline) {
        OUT_PUTC('\n');
    }
    
    return 0;
}
