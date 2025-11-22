/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "memalloc.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "ast.h"

int builtin_eval(char **argv) {
    // If no arguments, return 0
    if (!argv[1]) {
        return 0;
    }
    
    // Concatenate all arguments with spaces
    size_t total_len = 0;
    for (int i = 1; argv[i]; i++) {
        total_len += strlen(argv[i]) + 1; // +1 for space or null terminator
    }
    
    char *command = xmalloc(total_len);
    
    command[0] = '\0';
    for (int i = 1; argv[i]; i++) {
        if (i > 1) {
            strcat(command, " ");
        }
        strcat(command, argv[i]);
    }
    
    // Parse the command
    Lexer lexer;
    lexer_init(&lexer, command);
    ASTNode *ast = parser_parse(&lexer);
    
    int status = 0;
    if (ast) {
        // Execute the command
        status = executor_execute(ast);
        ast_free(ast);
    } else {
        // Parse error - in interactive mode, don't abort
        fprintf(stderr, "eval: parse error\n");
        status = 1;
    }
    
    free(command);
    return status;
}
