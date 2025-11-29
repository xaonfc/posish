/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "error.h"
#include "memalloc.h"
#include "memalloc.h"
#include "builtins.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "variables.h"

// Find file in PATH
static char *find_in_path(const char *filename) {
    const char *path = pathval();
    if (!path) return NULL;
    
    char *path_copy = xstrdup(path);
    char *dir = strtok(path_copy, ":");
    
    while (dir) {
        size_t len = strlen(dir) + strlen(filename) + 2;
        char *full_path = xmalloc(len);
        snprintf(full_path, len, "%s/%s", dir, filename);
        
        if (access(full_path, R_OK) == 0) {
            free(path_copy);
            return full_path;
        }
        
        free(full_path);
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return NULL;
}

int builtin_dot(char **args) {
    if (!args[1]) {
        error_msg(".: filename argument required");
        return 2;
    }
    
    char *filename = args[1];
    char *filepath = NULL;
    
    // If filename contains '/', use it directly
    if (strchr(filename, '/')) {
        filepath = xstrdup(filename);
    } else {
        // Search in PATH
        filepath = find_in_path(filename);
        if (!filepath) {
            error_msg(".: %s: not found", filename);
            return 1;
        }
    }
    
    // Check if file is readable
    if (access(filepath, R_OK) != 0) {
        error_sys(".: %s: cannot open file", filepath);
        free(filepath);
        return 1;
    }
    
    // Read file contents
    FILE *file = fopen(filepath, "r");
    if (!file) {
        error_sys(".: %s: cannot open file", filepath);
        free(filepath);
        return 1;
    }
    
    // Read entire file into buffer
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = mem_stack_alloc(file_size + 1);
    if (!content) {
        fclose(file);
        free(filepath);
        return 1;
    }
    
    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';
    fclose(file);
    free(filepath);
    
    // Parse and execute
    Lexer lexer;
    lexer_init(&lexer, content);
    
    struct stackmark smark;
    mem_stack_push_mark(&smark);
    
    ASTNode *ast = parser_parse(&lexer);
    
    int status = 0;
    if (ast) {
        status = executor_execute(ast);
        // ast_free(ast); // No-op
    }
    
    mem_stack_pop_mark(&smark);
    
    // No free needed for content
    return status;
}
