/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "executor.h"
#include "memalloc.h"
#include "memalloc.h"
#include "builtins.h"
#include "error.h"
#include "variables.h"
#include "jobs.h"
#include "lexer.h"
#include "shell_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <pwd.h>
#include "functions.h"
#include "signals.h"
#include "redirection.h"
#include "buf_output.h"

/* 
 * vfork() was removed from POSIX.1-2008, so _POSIX_C_SOURCE=200809L hides it.
 * We explicitly declare it here because we want the performance benefits
 * of vfork() on systems that support it (Linux, BSDs, QNX), even when compiling
 * in strict POSIX mode.
 * 
 * On QNX, vfork() is deprecated but still functional and faster than fork().
 * We suppress the deprecation warning to maintain performance.
 */
#if defined(__QNX__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
extern pid_t vfork(void);
#endif

extern char **environ;

static int last_exit_status = 0;
int func_return_status = 0;
int executor_break_count = 0;
int executor_continue_count = 0;

#include "signals.h"

int executor_get_last_status(void) {
    return last_exit_status;
}

void executor_set_last_status(int status) {
    last_exit_status = status;
}

// Helper to find executable in PATH
char *find_executable(const char *command) {
    if (strchr(command, '/')) {
        return mem_stack_strdup(command);
    }

    const char *path_env = pathval();
    if (!path_env) return NULL;

    char *path_copy = mem_stack_strdup(path_env);
    char *dir = strtok(path_copy, ":");
    char *result = NULL;

    while (dir) {
        size_t len = strlen(dir) + strlen(command) + 2;
        char *full_path = mem_stack_alloc(len);
        snprintf(full_path, len, "%s/%s", dir, command);

        if (access(full_path, X_OK) == 0) {
            result = full_path;
            break;
        }

        // No free needed for full_path
        dir = strtok(NULL, ":");
    }

    // No free needed for path_copy
    return result;
}



#include "parser.h"

static int is_safe_for_vfork(ASTNode *node);
char **expand_word_split(const char *word);

static char *execute_builtin_capture(char **argv) {
    // Create a pipe to capture stdout
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return mem_stack_strdup("");
    }
    
    // Save original stdout
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return mem_stack_strdup("");
    }
    
    // Redirect stdout to pipe
    if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
        close(saved_stdout);
        close(pipefd[0]);
        close(pipefd[1]);
        return mem_stack_strdup("");
    }
    close(pipefd[1]);
    
    // Execute builtin in-process
    builtin_run(argv);
    
    // Restore stdout
    buf_out_flush_all();
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    // Read captured output from pipe
    size_t size = 0;
    size_t capacity = 128;
    char *buffer = mem_stack_alloc(capacity);
    
    ssize_t n;
    while ((n = read(pipefd[0], buffer + size, capacity - size - 1)) > 0) {
        size += n;
        if (size >= capacity - 1) {
            size_t new_capacity = capacity * 2;
            buffer = mem_stack_realloc_array(buffer, capacity, new_capacity, 1);
            capacity = new_capacity;
        }
    }
    close(pipefd[0]);
    buffer[size] = '\0';
    
    // Strip trailing newlines
    while (size > 0 && buffer[size - 1] == '\n') {
        buffer[--size] = '\0';
    }
    
    return buffer;
}

static char *execute_subshell_capture(const char *cmd_str) {
    // ULTRA-FAST path: Skip parsing for known zero-output builtins
    // This avoids lexer/parser overhead for the most common cases
    if (cmd_str[0] == 't' && strcmp(cmd_str, "true") == 0) {
        char *argv[] = {"true", NULL};
        builtin_run(argv);
        return mem_stack_strdup("");
    }
    if (cmd_str[0] == 'f' && strcmp(cmd_str, "false") == 0) {
        char *argv[] = {"false", NULL};
        builtin_run(argv);
        return mem_stack_strdup("");
    }
    if (cmd_str[0] == ':' && cmd_str[1] == '\0') {
        char *argv[] = {":", NULL};
        builtin_run(argv);
        return mem_stack_strdup("");
    }
    
    // OPTIMIZATION: Parse first to check if we can avoid fork
    Lexer lexer;
    lexer_init(&lexer, cmd_str);
    ASTNode *node = parser_parse(&lexer);
    
    // Fast path: simple builtin with no args or redirections
    if (node && node->type == NODE_COMMAND && 
        node->data.command.arg_count == 1 &&  // Just the command name
        node->data.command.redirection_count == 0 &&
        builtin_is_builtin(node->data.command.args[0])) {
        
        const char *cmd_name = node->data.command.args[0];
        
        // ULTRA-FAST path: builtins that never produce output
        if (strcmp(cmd_name, "true") == 0 || 
            strcmp(cmd_name, "false") == 0 || 
            strcmp(cmd_name, ":") == 0) {
            // Execute directly, no pipe needed
            char *argv[2] = {(char *)cmd_name, NULL};
            builtin_run(argv);
            return mem_stack_strdup("");
        }

        // Check for other safe builtins (echo, printf, pwd)
        // Unsafe builtins (cd, exit, export, etc.) must fork to avoid polluting parent state
        int is_safe = (strcmp(cmd_name, "echo") == 0 || 
                       strcmp(cmd_name, "printf") == 0 || 
                       strcmp(cmd_name, "pwd") == 0);
                       
        if (!is_safe) {
            goto slow_path;
        }
        
        if (!is_safe) {
            goto slow_path;
        }
        
        char *argv[2] = {(char *)cmd_name, NULL};
        return execute_builtin_capture(argv);
    }
    
    // OPTIMIZATION: Check for simple functions that wrap safe builtins
    // e.g. func() { echo 1; }
    if (node && node->type == NODE_COMMAND && 
        node->data.command.arg_count == 1 && // No args passed to function
        node->data.command.redirection_count == 0 &&
        !builtin_is_builtin(node->data.command.args[0])) {
            
        ASTNode *body = func_get(node->data.command.args[0]);
        if (body) {
            // Unwrap group { ... }
            if (body->type == NODE_GROUP) body = body->data.group.body;
            
            // Check if body is a simple command
            if (body->type == NODE_COMMAND && 
                body->data.command.redirection_count == 0 &&
                body->data.command.assignment_count == 0) {
                
                const char *inner_cmd = body->data.command.args[0];
                
                // Check if inner command is a safe builtin
                int is_safe = (strcmp(inner_cmd, "echo") == 0 || 
                               strcmp(inner_cmd, "printf") == 0 || 
                               strcmp(inner_cmd, "pwd") == 0 ||
                               strcmp(inner_cmd, "true") == 0 ||
                               strcmp(inner_cmd, "false") == 0 ||
                               strcmp(inner_cmd, ":") == 0);
                               
                if (is_safe) {
                    // Expand arguments
                    char **argv = NULL;
                    size_t argc = 0;
                    
                    for (size_t i = 0; i < body->data.command.arg_count; i++) {
                        char **expanded = expand_word_split(body->data.command.args[i]);
                        if (expanded) {
                            for (int k = 0; expanded[k]; k++) {
                                argv = mem_stack_realloc_array(argv, argc, argc + 1, sizeof(char*));
                                argv[argc++] = expanded[k];
                            }
                        }
                    }
                    argv = mem_stack_realloc_array(argv, argc, argc + 1, sizeof(char*));
                    argv[argc] = NULL;
                    
                    return execute_builtin_capture(argv);
                }
            }
        }
    }
    
    slow_path:;
    // Slow path: fork required
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        error_sys("pipe");
        return mem_stack_strdup("");
    }

    // CRITICAL: Flush buffers before fork to prevent child from inheriting and flushing
    // parent's buffered output into the capture pipe.
    buf_out_flush_all();

    // Use vfork() if safe (no state modification), otherwise fork()
    // CRITICAL: Must check safety because vfork shares memory with parent!
    // Use fork() instead of vfork() to prevent memory corruption
    // vfork() shares address space, and child modifying stack/heap can corrupt parent
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        int status = executor_execute(node);
        // CRITICAL: Flush buffered output before _exit() so it goes to pipe
        buf_out_flush_all();
        // ast_free(node); // No-op
        _exit(status);  // CRITICAL: use _exit() not exit() with vfork()
    } else if (pid < 0) {
        error_sys("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return mem_stack_strdup("");
    }

    close(pipefd[1]);
    
    size_t size = 0;
    size_t capacity = 128;
    char *buffer = mem_stack_alloc(capacity);
    
    ssize_t n;
    while ((n = read(pipefd[0], buffer + size, capacity - size - 1)) > 0) {
        size += n;
        if (size >= capacity - 1) {
            // We can't easily realloc on stack without moving, but mem_stack_realloc_array handles it (by alloc new + copy)
            // Since we are just appending, it's fine.
            size_t new_capacity = capacity * 2;
            buffer = mem_stack_realloc_array(buffer, capacity, new_capacity, 1);
            capacity = new_capacity;
        }
    }
    close(pipefd[0]);
    buffer[size] = '\0';
    
    waitpid(pid, NULL, 0);
    
    while (size > 0 && buffer[size - 1] == '\n') {
        buffer[--size] = '\0';
    }
    
    return buffer;
}

static char *expand_tilde(const char *word) {
    if (word[0] != '~') {
        return mem_stack_strdup(word);
    }

    const char *slash = strchr(word, '/');
    size_t prefix_len = slash ? (size_t)(slash - word) : strlen(word);
    
    char *home = NULL;
    
    if (prefix_len == 1) {
        home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
    } else {
        char *username = mem_stack_alloc(prefix_len);
        strncpy(username, word + 1, prefix_len - 1);
        username[prefix_len - 1] = '\0';
        
        struct passwd *pw = getpwnam(username);
        if (pw) {
            home = pw->pw_dir;
        }
        // No free needed for username
    }
    
    if (home) {
        size_t home_len = strlen(home);
        size_t suffix_len = slash ? strlen(slash) : 0;
        char *result = mem_stack_alloc(home_len + suffix_len + 1);
        strcpy(result, home);
        if (slash) strcat(result, slash);
        return result;
    }
    
    return mem_stack_strdup(word);
}

static long eval_expression(const char **str);

// Pattern removal functions
static char *remove_suffix_shortest(const char *str, const char *pattern);
static char *remove_suffix_longest(const char *str, const char *pattern);
static char *remove_prefix_shortest(const char *str, const char *pattern);
static char *remove_prefix_longest(const char *str, const char *pattern);

static long eval_factor(const char **str) {
    while (isspace(**str)) (*str)++;
    
    if (**str == '(') {
        (*str)++;
        long val = eval_expression(str);
        while (isspace(**str)) (*str)++;
        if (**str == ')') (*str)++;
        return val;
    } else if (isalpha(**str) || **str == '_') {
        const char *start = *str;
        while (isalnum(**str) || **str == '_') (*str)++;
        size_t len = *str - start;
        char *name = mem_stack_alloc(len + 1);
        strncpy(name, start, len);
        name[len] = '\0';
        
        char *val_str = posish_var_get(name);

        long val = 0;
        if (val_str) {
            val = strtol(val_str, NULL, 10);
            free(val_str);
        }
        // No free needed for name
        return val;
    } else if (isdigit(**str) || **str == '-' || **str == '+') {
        char *end;
        long val = strtol(*str, &end, 10);
        *str = end;
        return val;
    }
    return 0;
}

static long eval_term(const char **str) {
    long val = eval_factor(str);
    while (1) {
        while (isspace(**str)) (*str)++;
        if (**str == '*') {
            (*str)++;
            val *= eval_factor(str);
        } else if (**str == '/') {
            (*str)++;
            long divisor = eval_factor(str);
            if (divisor == 0) {
                error_msg("division by 0");
                exit(1);
            }
            val /= divisor;
        } else if (**str == '%') {
            (*str)++;
            long divisor = eval_factor(str);
            if (divisor == 0) {
                error_msg("division by 0");
                exit(1);
            }
            val %= divisor;
        } else {
            break;
        }
    }
    return val;
}

static long eval_expression(const char **str) {
    long val = eval_term(str);
    while (1) {
        while (isspace(**str)) (*str)++;
        if (**str == '+') {
            (*str)++;
            val += eval_term(str);
        } else if (**str == '-') {
            (*str)++;
            val -= eval_term(str);
        } else {
            break;
        }
    }
    return val;
}

static long evaluate_arithmetic(const char *expr) {
    const char *str = expr;
    return eval_expression(&str);
}

static int is_ifs(char c, const char *ifs) {
    if (!ifs) {
        return c == ' ' || c == '\t' || c == '\n';
    }
    return strchr(ifs, c) != NULL;
}

static int is_ifs_whitespace(char c, const char *ifs) {
    if (!ifs) {
        return c == ' ' || c == '\t' || c == '\n';
    }
    return strchr(ifs, c) && isspace((unsigned char)c);
}

// StringBuilder for efficient string construction
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder *sb) {
    sb->cap = 64;
    sb->data = mem_stack_alloc(sb->cap);
    sb->len = 0;
    sb->data[0] = '\0';
}

static void sb_append(StringBuilder *sb, char c) {
    if (sb->len + 1 >= sb->cap) {
        size_t new_cap = sb->cap * 2;
        sb->data = mem_stack_realloc_array(sb->data, sb->cap, new_cap, 1);
        sb->cap = new_cap;
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static void sb_append_str(StringBuilder *sb, const char *s) {
    size_t slen = strlen(s);
    if (sb->len + slen + 1 >= sb->cap) {
        size_t new_cap = sb->cap;
        while (sb->len + slen + 1 >= new_cap) {
            new_cap *= 2;
        }
        sb->data = mem_stack_realloc_array(sb->data, sb->cap, new_cap, 1);
        sb->cap = new_cap;
    }
    strcpy(sb->data + sb->len, s);
    sb->len += slen;
}

static char *sb_finish(StringBuilder *sb) {
    return sb->data; // Transfer ownership (stack allocated)
}



char *expand_word(const char *word);

static char **expand_word_internal(const char *word, int allow_split) {
    if (!word) return NULL;
    
    char *tilde_expanded = expand_tilde(word);
    size_t len = strlen(tilde_expanded);
    
    char **results = NULL;
    size_t result_count = 0;
    
    StringBuilder sb;
    sb_init(&sb);
    
    size_t i = 0;
    const char *input = tilde_expanded;
    int saw_quotes = 0;
    
    const char *ifs = ifsval();
    
    int in_quote = 0;
    int push_empty_at_end = !allow_split;

    // Skip leading IFS whitespace if splitting (for the word itself)
    if (allow_split) {
        while (i < len && is_ifs_whitespace(input[i], ifs)) i++;
    }
    
    while (i < len) {
        if (input[i] == '\\') {
            push_empty_at_end = 1;
            if (in_quote == 0) {
                // Unquoted backslash: next char is literal
                if (i + 1 < len) {
                    sb_append(&sb, input[i+1]);
                    i += 2;
                } else {
                    sb_append(&sb, '\\');
                    i++;
                }
            } else if (in_quote == 2) {
                // Double quoted backslash
                if (i + 1 < len) {
                    char next = input[i+1];
                    if (next == '$' || next == '`' || next == '"' || next == '\\') {
                        sb_append(&sb, next);
                        i += 2;
                    } else {
                        sb_append(&sb, '\\');
                        sb_append(&sb, next);
                        i += 2;
                    }
                } else {
                    sb_append(&sb, '\\');
                    i++;
                }
            } else {
                // Single quoted backslash (literal)
                sb_append(&sb, '\\');
                i++;
            }
        } else if (input[i] == '\'') {
            push_empty_at_end = 1;
            saw_quotes = 1;
            if (in_quote == 0) {
                i++;
                while (i < len && input[i] != '\'') {
                    sb_append(&sb, input[i++]);
                }
                if (i < len) i++;
            } else {
                sb_append(&sb, input[i++]);
            }
        } else if (input[i] == '"') {
            push_empty_at_end = 1;
            saw_quotes = 1;
            if (in_quote == 0) {
                in_quote = 2;
                i++;
            } else if (in_quote == 2) {
                in_quote = 0;
                i++;
            } else {
                sb_append(&sb, input[i++]);
            }
        } else if (input[i] == '$') {
            push_empty_at_end = 1;
            if (i + 1 < len && input[i+1] == '(' && i + 2 < len && input[i+2] == '(') {
                // Arithmetic $((...))  
                i += 3; // Skip $((
                size_t start = i;
                
                int nesting = 0;
                while (i + 1 < len) {
                    if (input[i] == '(') nesting++;
                    else if (input[i] == ')') {
                        if (nesting > 0) nesting--;
                        else {
                            // Found first closing paren of `))`.
                            if (input[i+1] == ')') {
                                break;
                            }
                        }
                    }
                    i++;
                }
                
                if (i + 1 < len && input[i] == ')' && input[i+1] == ')') {
                    size_t expr_len = i - start;
                    char *expr = mem_stack_alloc(expr_len + 1);
                    strncpy(expr, input + start, expr_len);
                    expr[expr_len] = '\0';
                    
                    char *expanded_expr = expand_word(expr);
                    long val = evaluate_arithmetic(expanded_expr);
                    // No free needed for expr, expanded_expr
                    char val_str[32];
                    snprintf(val_str, sizeof(val_str), "%ld", val);
                    
                    if (allow_split && in_quote == 0) {
                        const char *p = val_str;
                        while (*p) {
                            if (is_ifs(*p, ifs)) {
                                if (is_ifs_whitespace(*p, ifs) && result_count == 0 && sb.len == 0) {
                                     p++;
                                     while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                     continue;
                                }
                                
                                results = xrealloc(results, (result_count + 2) * sizeof(char *));
                                results[result_count++] = sb_finish(&sb);
                                sb_init(&sb);
                                
                                if (is_ifs_whitespace(*p, ifs)) {
                                    while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                    if (*p && is_ifs(*p, ifs) && !is_ifs_whitespace(*p, ifs)) {
                                        p++;
                                        while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                    }
                                    push_empty_at_end = 0;
                                } else {
                                    p++;
                                    while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                    push_empty_at_end = 1;
                                }
                            } else {
                                sb_append(&sb, *p++);
                                push_empty_at_end = 1;
                            }
                        }
                    } else {
                        sb_append_str(&sb, val_str);
                    }
                    i += 2; // Skip `))`
                } else {
                    // Error: missing ))
                    i++;
                }
            } else if (i + 1 < len && input[i+1] == '(') {
                i += 2;
                size_t start = i;
                int nesting = 1;
                while (i < len && nesting > 0) {
                    if (input[i] == '(') nesting++;
                    else if (input[i] == ')') nesting--;
                    i++;
                }
                if (nesting == 0) {
                    size_t cmd_len = i - 1 - start;
                    char *cmd = mem_stack_alloc(cmd_len + 1);
                    strncpy(cmd, input + start, cmd_len);
                    cmd[cmd_len] = '\0';
                    char *output = execute_subshell_capture(cmd);
                    
                    if (allow_split && in_quote == 0) {
                         const char *p = output;
                        while (*p) {
                            if (is_ifs(*p, ifs)) {
                                if (is_ifs_whitespace(*p, ifs) && result_count == 0 && sb.len == 0) {
                                     p++;
                                     while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                     continue;
                                }

                                results = xrealloc(results, (result_count + 2) * sizeof(char *));
                                results[result_count++] = sb_finish(&sb);
                                sb_init(&sb);
                                
                                if (is_ifs_whitespace(*p, ifs)) {
                                    while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                    if (*p && is_ifs(*p, ifs) && !is_ifs_whitespace(*p, ifs)) {
                                        p++;
                                        while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                    }
                                    push_empty_at_end = 0;
                                } else {
                                    p++;
                                    while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                    push_empty_at_end = 1;
                                }
                            } else {
                                sb_append(&sb, *p++);
                                push_empty_at_end = 1;
                            }
                        }
                    } else {
                        sb_append_str(&sb, output);
                    }
                    // No free needed for cmd, output
                }
            } else {
                i++; 
                char *var_name = NULL;
                size_t var_len = 0;
                
                if (i < len && input[i] == '{') {
                    i++;
                    size_t start = i;
                    
                    // Check for ${#VAR} - length of variable
                    int is_length = 0;
                    if (i < len && input[i] == '#') {
                        is_length = 1;
                        i++; // Skip the #
                        start = i;  // Start after the #
                    }
                    
                    // Find the variable name and operator
                    // If is_length, we only look for closing }
                    if (is_length) {
                        while (i < len && input[i] != '}') {
                            i++;
                        }
                    } else {
                        while (i < len && input[i] != '}' && input[i] != ':' && input[i] != '%' && input[i] != '#') {
                            i++;
                        }
                    }
                    
                    var_len = i - start;
                    var_name = xmalloc(var_len + 1);
                    memcpy(var_name, input + start, var_len);
                    var_name[var_len] = '\0';
                    
                    // If it's a length operation, get the value and append its length
                    if (is_length) {
                        const char *val = posish_var_get_value(var_name);
                        int length = val ? strlen(val) : 0;
                        char len_buf[32];
                        snprintf(len_buf, sizeof(len_buf), "%d", length);
                        sb_append_str(&sb, len_buf);
                        free(var_name);
                        if (i < len && input[i] == '}') i++; // Skip closing }
                        continue;
                    }
                    
                    
                    // Check for parameter expansion modifiers
                    char *default_value = NULL;
                    int colon_op = 0; // 0=none, 1=:-, 2=:+, 3=:=, 4=:?
                    char *pattern = NULL;
                    int pattern_op = 0; // 0=none, 1=%, 2=%%, 3=#, 4=##
                    
                    if (i < len && input[i] == ':') {
                        i++; // Skip ':'
                        if (i < len && (input[i] == '-' || input[i] == '+' || input[i] == '=' || input[i] == '?')) {
                            char op_char = input[i];
                            i++; // Skip operator char
                            
                            // Map operator to colon_op
                            if (op_char == '-') colon_op = 1;
                            else if (op_char == '+') colon_op = 2;
                            else if (op_char == '=') colon_op = 3;
                            else if (op_char == '?') colon_op = 4;
                            
                            // Get the default value/message
                            size_t val_start = i;
                            while (i < len && input[i] != '}') i++;
                            size_t val_len = i - val_start;
                            default_value = mem_stack_alloc(val_len + 1);
                            memcpy(default_value, input + val_start, val_len);
                            default_value[val_len] = '\0';
                        } else {
                            // Invalid syntax like ${var:2} (ksh/bash substring not supported)
                            error_msg("Bad substitution");
                            exit(2);
                        }
                    } else if (i < len && (input[i] == '%' || input[i] == '#')) {
                        char op_char = input[i];
                        i++;
                        
                        // Check for double operator (%% or ##)
                        if (i < len && input[i] == op_char) {
                            pattern_op = (op_char == '%') ? 2 : 4;
                            i++;
                        } else {
                            pattern_op = (op_char == '%') ? 1 : 3;
                        }
                        
                        // Get the pattern
                        size_t pat_start = i;
                        while (i < len && input[i] != '}') i++;
                        size_t pat_len = i - pat_start;
                        pattern = mem_stack_alloc(pat_len + 1);
                        memcpy(pattern, input + pat_start, pat_len);
                        pattern[pat_len] = '\0';
                    } else {
                        // No operator, skip to }
                        while (i < len && input[i] != '}') i++;
                    }
                    
                    if (i < len) i++; // Skip '}'
                    
                    // Get variable value
                    const char *var_value = NULL;
                    if (strcmp(var_name, "?") == 0) {
                        static char buf[32]; snprintf(buf, sizeof(buf), "%d", executor_get_last_status());
                        var_value = buf;
                    } else if (strcmp(var_name, "$") == 0) {
                        static char buf[32]; snprintf(buf, sizeof(buf), "%d", getpid());
                        var_value = buf;
                    } else if (strcmp(var_name, "-") == 0) {
                        var_value = "im";
                    } else if (isdigit(var_name[0])) {
                        var_value = posish_var_get_positional_value(atoi(var_name));
                    } else {
                        var_value = posish_var_get_value(var_name);
                    }
                    
                    // Handle colon operators
                    if (colon_op) {
                        int is_unset_or_null = (!var_value || var_value[0] == '\0');
                        
                        switch (colon_op) {
                            case 1: // :- Use default if unset or null
                                if (is_unset_or_null) {
                                    char *expanded_default = expand_word(default_value);
                                    var_value = expanded_default;
                                }
                                break;
                            case 2: // :+ Use alternative if set and not null
                                if (!is_unset_or_null) {
                                    char *expanded_alt = expand_word(default_value);
                                    var_value = expanded_alt;
                                } else {
                                    var_value = "";
                                }
                                break;
                            case 3: // := Assign default if unset or null
                                if (is_unset_or_null) {
                                    char *expanded_default = expand_word(default_value);
                                    posish_var_set(var_name, expanded_default);
                                    var_value = posish_var_get_value(var_name);
                                }
                                break;
                            case 4: // :? Error if unset or null
                                if (is_unset_or_null) {
                                    char *msg = default_value && default_value[0] ? default_value : "parameter null or not set";
                                    error_msg("%s: %s", var_name, msg);
                                    exit(1);
                                }
                                break;
                        }
                    }
                    
                    // Apply pattern removal if needed
                    if (pattern_op && var_value) {
                        char *result = NULL;
                        switch (pattern_op) {
                            case 1: // % - remove shortest suffix
                                result = remove_suffix_shortest(var_value, pattern);
                                break;
                            case 2: // %% - remove longest suffix
                                result = remove_suffix_longest(var_value, pattern);
                                break;
                            case 3: // # - remove shortest prefix
                                result = remove_prefix_shortest(var_value, pattern);
                                break;
                            case 4: // ## - remove longest prefix
                                result = remove_prefix_longest(var_value, pattern);
                                break;
                        }
                        if (result) {
                            var_value = result;
                        }
                    }
                    
                    if (var_value) {
                        sb_append_str(&sb, var_value);
                    }
                    // if (pattern) free(pattern); // No free needed
                    // if (default_value) free(default_value); // No free needed
                    free(var_name); // Free xmalloc'd var_name
                    var_name = NULL;
                    continue;
                } else {
                    size_t start = i;
                    if (i < len && (input[i] == '?' || input[i] == '$' || input[i] == '#' || input[i] == '!' || input[i] == '@' || input[i] == '*' || input[i] == '-' || isdigit(input[i]))) {
                        i++;
                    } else {
                        while (i < len && (isalnum(input[i]) || input[i] == '_')) i++;
                    }
                    var_len = i - start;
                    
                    // If variable name is empty, treat $ as literal
                    if (var_len == 0) {
                        sb_append(&sb, '$');
                        free(var_name);
                        var_name = NULL;
                        continue;
                    }
                    
                    var_name = xmalloc(var_len + 1);
                    memcpy(var_name, input + start, var_len);
                    var_name[var_len] = '\0';
                }
                
                    if (var_name) {
                        const char *val = NULL;
                    
                    if (strcmp(var_name, "?") == 0) {
                        static char buf[32]; snprintf(buf, sizeof(buf), "%d", executor_get_last_status());
                        val = buf;
                    } else if (strcmp(var_name, "$") == 0) {
                        static char buf[32]; snprintf(buf, sizeof(buf), "%d", getpid());
                        val = buf;
                    } else if (strcmp(var_name, "-") == 0) {
                        val = "im";
                    } else if (strcmp(var_name, "#") == 0) {
                        static char buf[32]; snprintf(buf, sizeof(buf), "%d", posish_var_get_positional_count());
                        val = buf;
                    } else if (strcmp(var_name, "!") == 0) {
                        pid_t bg_pid = posish_var_get_last_bg_pid();
                        static char buf[32]; if (bg_pid > 0) snprintf(buf, sizeof(buf), "%d", bg_pid); else buf[0] = '\0';
                        val = buf;
                    } else if (strcmp(var_name, "@") == 0 || strcmp(var_name, "*") == 0) {
                        char **args = posish_var_get_all_positional();
                        if (args) {
                            size_t total_len = 0;
                            for (int k = 0; args[k]; k++) total_len += strlen(args[k]) + 1;
                            char *tmp_val = mem_stack_alloc(total_len + 1);
                            char *p = tmp_val;
                            for (int k = 0; args[k]; k++) {
                                size_t arg_len = strlen(args[k]);
                                memcpy(p, args[k], arg_len);
                                p += arg_len;
                                if (args[k+1]) *p++ = ' ';
                                free(args[k]);
                            }
                            *p = '\0';
                            free(args);
                            val = tmp_val; // This is stack allocated, so it's fine.
                        } else { val = ""; }
                    } else if (isdigit(var_name[0])) {
                        val = posish_var_get_positional_value(atoi(var_name));
                    } else {
                        val = posish_var_get_value(var_name);
                    }
                    
                    if (val) {
                        if (allow_split && in_quote == 0) {
                            const char *p = val;
                            while (*p) {
                                if (is_ifs(*p, ifs)) {
                                    if (is_ifs_whitespace(*p, ifs) && result_count == 0 && sb.len == 0) {
                                         p++;
                                         while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                         continue;
                                    }

                                    results = mem_stack_realloc_array(results, result_count * sizeof(char*), (result_count + 2) * sizeof(char *), 1);
                                    results[result_count++] = sb_finish(&sb);
                                    sb_init(&sb);
                                    
                                    if (is_ifs_whitespace(*p, ifs)) {
                                        while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                        if (*p && is_ifs(*p, ifs) && !is_ifs_whitespace(*p, ifs)) {
                                            p++;
                                            while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                        }
                                        push_empty_at_end = 0;
                                    } else {
                                        p++;
                                        while (*p && is_ifs_whitespace(*p, ifs)) p++;
                                        push_empty_at_end = 1;
                                    }
                                } else {
                                    sb_append(&sb, *p++);
                                    push_empty_at_end = 1;
                                }
                            }
                        } else {
                            sb_append_str(&sb, val);
                        }
                    }
                    free(var_name); // Free xmalloc'd var_name
                    var_name = NULL;
                }
            }
        } else if (input[i] == '`') {
             i++;
             size_t start = i;
             while (i < len && input[i] != '`') {
                 if (input[i] == '\\' && i + 1 < len && input[i+1] == '`') i += 2;
                 else i++;
             }
             size_t cmd_len = i - start;
             char *cmd = mem_stack_alloc(cmd_len + 1);
             memcpy(cmd, input + start, cmd_len);
             cmd[cmd_len] = '\0';
             if (i < len) i++; 
             
             char *output = execute_subshell_capture(cmd);
             
             if (allow_split && in_quote == 0) {
                 const char *p = output;
                while (*p) {
                    if (is_ifs(*p, ifs)) {
                        if (is_ifs_whitespace(*p, ifs) && result_count == 0 && sb.len == 0) {
                             p++;
                             while (*p && is_ifs_whitespace(*p, ifs)) p++;
                             continue;
                        }

                        results = mem_stack_realloc_array(results, result_count * sizeof(char*), (result_count + 2) * sizeof(char *), 1);
                        results[result_count++] = sb_finish(&sb);
                        sb_init(&sb);
                        
                        if (is_ifs_whitespace(*p, ifs)) {
                            while (*p && is_ifs_whitespace(*p, ifs)) p++;
                            if (*p && is_ifs(*p, ifs) && !is_ifs_whitespace(*p, ifs)) {
                                p++;
                                while (*p && is_ifs_whitespace(*p, ifs)) p++;
                            }
                            push_empty_at_end = 0;
                        } else {
                            p++;
                            while (*p && is_ifs_whitespace(*p, ifs)) p++;
                            push_empty_at_end = 1;
                        }
                    } else {
                        sb_append(&sb, *p++);
                        push_empty_at_end = 1;
                    }
                }
             } else {
                 sb_append_str(&sb, output);
             }
             // free(cmd); // No free needed
             // free(output); // No free needed
        } else {
            if (allow_split && in_quote == 0 && is_ifs(input[i], ifs)) {
                results = mem_stack_realloc_array(results, result_count * sizeof(char*), (result_count + 2) * sizeof(char *), 1);
                results[result_count++] = sb_finish(&sb);
                sb_init(&sb);
                
                if (is_ifs_whitespace(input[i], ifs)) {
                    i++;
                    while (i < len && is_ifs_whitespace(input[i], ifs)) i++;
                    if (i < len && is_ifs(input[i], ifs) && !is_ifs_whitespace(input[i], ifs)) {
                        i++;
                        while (i < len && is_ifs_whitespace(input[i], ifs)) i++;
                    }
                    push_empty_at_end = 0;
                } else {
                    i++;
                    while (i < len && is_ifs_whitespace(input[i], ifs)) i++;
                    push_empty_at_end = 1;
                }
            } else {
                sb_append(&sb, input[i++]);
                push_empty_at_end = 1;
            }
        }
    }
    
    // Only push the final result if:
    // 1. push_empty_at_end is true (we found quoted content or non-whitespace)
    // 2. AND either we're not splitting, OR the result is non-empty, OR we already have results, OR we saw quotes
    // This ensures unquoted ${VAR:-} that expands to empty produces zero args, not one empty arg
    // But quoted "" produces one empty arg
    if (push_empty_at_end && (!allow_split || sb.len > 0 || result_count > 0 || saw_quotes)) {
        results = mem_stack_realloc_array(results, result_count * sizeof(char*), (result_count + 2) * sizeof(char *), 1);
        results[result_count++] = sb_finish(&sb);
    } else {
        // free(sb.data); // No free needed
    }
    
    if (results) results[result_count] = NULL;
    
    // if (ifs_val) free(ifs_val); // No longer needed
    // free(tilde_expanded); // No free needed
    
    return results;
}

static int has_special_chars(const char *str);

char *expand_word(const char *word) {
    if (!has_special_chars(word)) {
        return (char *)word;
    }
    char **res_list = expand_word_internal(word, 0);
    if (!res_list) return NULL;
    char *res = res_list[0];
    return res;
}

static char **expand_simple_var(const char *word) {
    // Optimization for "$VAR"
    if (word[0] == '"' && word[1] == '$') {
        size_t len = strlen(word);
        if (len > 3 && word[len-1] == '"') {
            // Check if valid var name in between
            int is_simple = 1;
            for (size_t i = 2; i < len - 1; i++) {
                if (!isalnum(word[i]) && word[i] != '_') {
                    is_simple = 0;
                    break;
                }
            }
            if (is_simple) {
                // Extract var name
                char var_name[256];
                if (len - 3 < sizeof(var_name)) {
                    memcpy(var_name, word + 2, len - 3);
                    var_name[len - 3] = '\0';
                    
                    const char *val = posish_var_get_value(var_name);
                    char **res = mem_stack_alloc(2 * sizeof(char*));
                    res[0] = val ? mem_stack_strdup(val) : mem_stack_strdup("");
                    res[1] = NULL;
                    return res;
                }
            }
        }
    }
    return NULL;
}

char **expand_word_split(const char *word) {
    char **simple_res = expand_simple_var(word);
    if (simple_res) return simple_res;
    return expand_word_internal(word, 1);
}

static int has_glob_chars(const char *str) {
    int in_single = 0;
    int in_double = 0;
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '\\') { i++; continue; }
        if (c == '\'') { in_single = !in_single; continue; }
        if (c == '"') { in_double = !in_double; continue; }
        if (!in_single && !in_double) {
            if (c == '*' || c == '?') return 1;
            if (c == '[') {
                // Only a glob if there is a closing ']'
                // Scan ahead
                for (size_t j = i + 1; j < len; j++) {
                    if (str[j] == ']') return 1;
                }
            }
        }
    }
    return 0;
}

static int has_special_chars(const char *str) {
    // Check for characters that trigger expansion: $ ` \ ' " ~
    // Also glob chars if we were doing globbing, but expand_word handles that separately?
    // expand_word returns a single string, so globbing is not performed here?
    // Wait, expand_word_internal DOES handle globbing? No, execute_simple_command handles globbing.
    // expand_word_internal handles variable expansion, command substitution, quotes.
    
    for (const char *p = str; *p; p++) {
        if (*p == '$' || *p == '`' || *p == '\\' || *p == '\'' || *p == '"') return 1;
        if (p == str && *p == '~') return 1;
    }
    return 0;
}

static char *prepare_glob_pattern(const char *str) {
    size_t len = strlen(str);
    char *res = mem_stack_alloc(len * 2 + 1);
    size_t res_idx = 0;
    int in_single = 0;
    int in_double = 0;
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (in_single) {
            if (c == '\'') in_single = 0;
            else { res[res_idx++] = '\\'; res[res_idx++] = c; }
        } else if (in_double) {
            if (c == '"') in_double = 0;
            else if (c == '\\') {
                if (i + 1 < len) {
                    char next = str[i+1];
                    if (next == '$' || next == '`' || next == '"' || next == '\\') {
                        res[res_idx++] = '\\'; res[res_idx++] = next; i++;
                    } else {
                        res[res_idx++] = '\\'; res[res_idx++] = '\\'; res[res_idx++] = '\\'; res[res_idx++] = next; i++;
                    }
                }
            } else { res[res_idx++] = '\\'; res[res_idx++] = c; }
        } else {
            if (c == '\\') {
                if (i + 1 < len) { res[res_idx++] = '\\'; res[res_idx++] = str[i+1]; i++; }
            } else if (c == '\'') in_single = 1;
            else if (c == '"') in_double = 1;
            else res[res_idx++] = c;
        }
    }
    res[res_idx] = '\0';
    return res;
}

static int try_test_fast_path(int argc, char **argv) {
    const char *cmd = argv[0];
    
    // Fast-path for [ and test builtins
    if ((cmd[0] == '[' && cmd[1] == '\0') || (strcmp(cmd, "test") == 0)) {
        int is_bracket = (cmd[0] == '[');
        int effective_argc = argc;
        
        if (is_bracket) {
            if (strcmp(argv[argc-1], "]") != 0) {
                // Missing closing bracket, fall back to slow path for error reporting
                return -1;
            }
            effective_argc--; // Ignore trailing ]
        }
        
        // [ a = b ] or test a = b
        if (effective_argc == 4 && argv[2][1] == '\0') {
            if (argv[2][0] == '=') {
                return (strcmp(argv[1], argv[3]) != 0);
            }
        }
        
        // [ a != b ] or test a != b
        if (effective_argc == 4 && argv[2][0] == '!' && argv[2][1] == '=' && argv[2][2] == '\0') {
            return (strcmp(argv[1], argv[3]) == 0);
        }
        
        // [ -z a ] or test -z a
        if (effective_argc == 3 && argv[1][0] == '-' && argv[1][1] == 'z' && argv[1][2] == '\0') {
            return (argv[2][0] != '\0');
        }
        
        // [ -n a ] or test -n a
        if (effective_argc == 3 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0') {
            return (argv[2][0] == '\0');
        }
    }
    return -1;
}

static int execute_simple_command(ASTNode *node);
static int execute_pipeline(ASTNode *node);
static int execute_list(ASTNode *node);
static int execute_if(ASTNode *node);
static int execute_while(ASTNode *node);
static int execute_and_or(ASTNode *node);
static int execute_for(ASTNode *node);

static int execute_simple_command(ASTNode *node) {
    if (!node || node->type != NODE_COMMAND) return 1;




    
    for (size_t i = 0; i < node->data.command.assignment_count; i++) {
        char *expanded_val = expand_word(node->data.command.assignments[i].value);
        if (!expanded_val) {
            // Expansion failed (e.g. unbound variable)
            return 1;
        }
        if (posish_var_set(node->data.command.assignments[i].name, expanded_val) != 0) {
            // Assignment failed (readonly variable)
            return 1;
        }
        // No free needed for expanded_val
    }
    


    if (node->data.command.arg_count == 0) {
        return 0;
    }

    size_t argv_cap = node->data.command.arg_count + 4; // Pre-allocate with slight buffer
    char **argv = mem_stack_alloc(argv_cap * sizeof(char*));
    size_t argc = 0;
    
    for (size_t i = 0; i < node->data.command.arg_count; i++) {
        char **expanded_list = expand_word_split(node->data.command.args[i]);
        if (!expanded_list) continue;
        
        for (int k = 0; expanded_list[k]; k++) {
            char *expanded = expanded_list[k];
            
            if (has_glob_chars(expanded)) {
                char *pattern = prepare_glob_pattern(expanded);
                glob_t glob_result;
                int flags = GLOB_NOCHECK; 
                
                if (glob(pattern, flags, NULL, &glob_result) == 0) {
                    if (argc + glob_result.gl_pathc + 1 >= argv_cap) {
                        size_t new_cap = argv_cap * 2 + glob_result.gl_pathc;
                        argv = mem_stack_realloc_array(argv, argv_cap * sizeof(char*), new_cap * sizeof(char*), 1);
                        argv_cap = new_cap;
                    }
                    for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                        argv[argc++] = mem_stack_strdup(glob_result.gl_pathv[j]);
                    }
                    globfree(&glob_result);
                } else {
                    if (argc + 2 >= argv_cap) {
                        size_t new_cap = argv_cap * 2;
                        argv = mem_stack_realloc_array(argv, argv_cap * sizeof(char*), new_cap * sizeof(char*), 1);
                        argv_cap = new_cap;
                    }
                    argv[argc++] = expanded;
                    expanded = NULL;
                }
                // No free needed for pattern
            } else {
                if (argc + 2 >= argv_cap) {
                    size_t new_cap = argv_cap * 2;
                    argv = mem_stack_realloc_array(argv, argv_cap * sizeof(char*), new_cap * sizeof(char*), 1);
                    argv_cap = new_cap;
                }
                argv[argc++] = expanded;
                expanded = NULL;
            }
            // No free needed for expanded
        }
        // No free needed for expanded_list
    }
    if (argv) argv[argc] = NULL;

    // Empty command after expansion (all words expanded to nothing)
    if (argc == 0) {
        return 0;
    }

    // Trace mode (set -x) - Print expanded command
    if (shell_trace_mode && argv[0]) {
        char *ps4 = posish_var_get("PS4");
        fprintf(stderr, "%s", ps4 ? ps4 : "+ ");
        if (ps4) free(ps4); // posish_var_get returns malloced string
        
        for (size_t i = 0; i < argc; i++) {
            if (i > 0) fprintf(stderr, " ");
            fprintf(stderr, "%s", argv[i]);
        }
        fprintf(stderr, "\n");
    }

    ASTNode *func_body = func_get(argv[0]);
    if (func_body) {
        int has_redirections = (node->data.command.redirection_count > 0);
        int saved_stdin = -1, saved_stdout = -1, saved_stderr = -1;

        // Only save FDs if we have redirections
        if (has_redirections) {
            saved_stdin = dup(STDIN_FILENO);
            saved_stdout = dup(STDOUT_FILENO);
            saved_stderr = dup(STDERR_FILENO);
        }

        if (has_redirections && handle_redirections(node->data.command.redirections, node->data.command.redirection_count) != 0) {
            // No free needed for argv
            buf_out_flush_all();
            dup2(saved_stdin, STDIN_FILENO);
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stdin);
            close(saved_stdout);
            close(saved_stderr);
            return 1;
        }

        // Zero-copy save (just swap pointers)
        PositionalSave saved_params = posish_var_save_positional_fast();

        if (argc > 1) {
            posish_var_set_positional(argc - 1, argv + 1);
        } else {
            posish_var_set_positional(0, NULL);
        }
        
        // Push scope for function-local variables
        posish_var_push_scope();
        
        int status = executor_execute(func_body);
        
        // Pop scope to cleanup local variables
        posish_var_pop_scope();
        
        if (status == EXIT_RETURN) {
            status = func_return_status;
        }
        
        // Zero-copy restore (just swap back)
        posish_var_restore_positional_fast(saved_params);

        // Only restore FDs if we saved them
        if (has_redirections) {
            buf_out_flush_all();
            dup2(saved_stdin, STDIN_FILENO);
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stdin);
            close(saved_stdout);
            close(saved_stderr);
        }

        // for (size_t i = 0; i < argc; i++) free(argv[i]);
        // free(argv);
        return status;
    }


    // Fast-path for trivial builtins to avoid dispatcher overhead
    // Design inspired by FreeBSD sh architecture (BSD-3-Clause)
    const char *cmd = argv[0];
    if (cmd[0] == ':' && cmd[1] == '\0') {
        // ":" builtin - always succeeds
        // No free needed for argv
        return 0;
    }
    if (cmd[0] == 't' && strcmp(cmd, "true") == 0) {
        // No free needed for argv
        return 0;
    }
    if (cmd[0] == 'f' && strcmp(cmd, "false") == 0) {
        // No free needed for argv
        return 1;
    }

    int test_res = try_test_fast_path(argc, argv);
    if (test_res != -1) return test_res;

    // Builtin execution with robust argument handling
    if (builtin_is_builtin(argv[0])) {
        // CRITICAL FIX: Copy all argv strings to heap to isolate from mem_stack
        // The mem_stack allocator can be reset or reused during builtin execution
        // (e.g. in loops), invalidating stack pointers.
        char **heap_argv = xmalloc((argc + 1) * sizeof(char*));
        for (size_t i = 0; i < argc; i++) {
            heap_argv[i] = xstrdup(argv[i]);
        }
        heap_argv[argc] = NULL;

        int has_redirections = (node->data.command.redirection_count > 0);
        int saved_stdin = -1, saved_stdout = -1, saved_stderr = -1;

        if (has_redirections) {
            saved_stdin = dup(STDIN_FILENO);
            saved_stdout = dup(STDOUT_FILENO);
            saved_stderr = dup(STDERR_FILENO);
        }

        if (has_redirections && handle_redirections(node->data.command.redirections, node->data.command.redirection_count) != 0) {
            if (has_redirections) {
                buf_out_flush_all();
                dup2(saved_stdin, STDIN_FILENO);
                dup2(saved_stdout, STDOUT_FILENO);
                dup2(saved_stderr, STDERR_FILENO);
                close(saved_stdin);
                close(saved_stdout);
                close(saved_stderr);
            }
            // Free heap copy
            for (size_t i = 0; i < argc; i++) {
                free(heap_argv[i]);
            }
            free(heap_argv);
            return 1;
        }

        int status = builtin_run(heap_argv);
        
        if (has_redirections) {
            buf_out_flush_all();
            dup2(saved_stdin, STDIN_FILENO);
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stdin);
            close(saved_stdout);
            close(saved_stderr);
        }

        // Free heap copy
        for (size_t i = 0; i < argc; i++) {
            free(heap_argv[i]);
        }
        free(heap_argv);
        
        return status;
    }

    // External command execution
    char *executable = find_executable(argv[0]);
    if (!executable) {
        fprintf(stderr, "%s: command not found\n", argv[0]);
        return 127;
    }

    // Block SIGCHLD to prevent race condition where signal handler reaps process
    // before job_wait() can.
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // Prepare environment in parent to avoid heap allocation in child (vfork safety)
    char **env = posish_var_get_environ();

    // Use fork() instead of vfork() for safety
    // vfork() shares address space and is dangerous if child modifies state
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - restore signal mask
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        if (handle_redirections(node->data.command.redirections, 
                              node->data.command.redirection_count) != 0) {
            _exit(1);
        }

        execve(executable, argv, env);
        // execve failed - print appropriate error
        if (errno == ENOENT) {
            dprintf(STDERR_FILENO, "%s: %s: not found\n", "posish", executable);
            _exit(127);
        } else if (errno == EACCES) {
            dprintf(STDERR_FILENO, "%s: %s: Permission denied\n", "posish", executable);
            _exit(126);
        } else {
            dprintf(STDERR_FILENO, "%s: %s: %s\n", "posish", executable, strerror(errno));
            _exit(126);
        }
    } else if (pid < 0) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL); // Restore mask on error
        error_sys("fork");
        // Free env
        for (int i = 0; env[i]; i++) free(env[i]);
        free(env);
        return 1;
    }

    // Parent process
    
    // Free environment array (deep free)
    for (int i = 0; env[i]; i++) free(env[i]);
    free(env);

    // Only set process group if job control is enabled
    // Otherwise external commands should share the shell's PGID
    if (shell_monitor) {
        setpgid(pid, pid);
    }
    
    Job *j = job_add(pid, argv[0], JOB_RUNNING);
    posish_var_set_last_bg_pid(pid);
    
    int status = job_wait(j);
    
    // Restore signal mask
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    
    // If child was interrupted by SIGINT, propagate it
    if (status == 128 + SIGINT) {
        got_sigint = 1;
    }
    
    return status;
}

static int execute_pipeline(ASTNode *node) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("posish: pipe failed");
        return 1;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        exit(executor_execute(node->data.pipeline.left));
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        exit(executor_execute(node->data.pipeline.right));
    }

    close(pipefd[0]);
    close(pipefd[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    if (WIFEXITED(status2)) {
        return WEXITSTATUS(status2);
    }
    return 1;
}

static int execute_list(ASTNode *node) {
    int status = 0;
    


    if (node->data.list.left) {
        if (node->data.list.async) {
            pid_t pid = fork();
            if (pid == 0) {
                setpgid(0, 0);
                exit(executor_execute(node->data.list.left));
            } else if (pid < 0) {
                perror("posish: fork failed");
            } else {
                setpgid(pid, pid);
                Job *j = job_add(pid, "background task", JOB_RUNNING);
                printf("[%d] %d\n", j->id, pid);
                posish_var_set_last_bg_pid(pid);
                status = 0;
            }
        } else {
            status = executor_execute(node->data.list.left);
            if (status == EXIT_BREAK || status == EXIT_CONTINUE || status == EXIT_RETURN) {
                return status;
            }
        }
    }
    
    if (node->data.list.right) {

        status = executor_execute(node->data.list.right);
    } else {

    }
    
    return status;
}

static int execute_for(ASTNode *node) {
    char **items = NULL;
    size_t item_count = 0;
    
    if (node->data.for_loop.word_list) {
        for (size_t i = 0; i < node->data.for_loop.word_count; i++) {
            char **expanded_list = expand_word_split(node->data.for_loop.word_list[i]);
            if (expanded_list) {
                for (int k = 0; expanded_list[k]; k++) {
                    char *expanded = expanded_list[k];
                    // fprintf(stderr, "For loop item: %s\n", expanded);
                    
                    if (has_glob_chars(expanded)) {
                        char *pattern = prepare_glob_pattern(expanded);
                        glob_t glob_result;
                        int flags = GLOB_NOCHECK;
                        
                        if (glob(pattern, flags, NULL, &glob_result) == 0) {
                            items = mem_stack_realloc_array(items, item_count * sizeof(char*), (item_count + glob_result.gl_pathc) * sizeof(char *), 1);
                            for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                                items[item_count++] = mem_stack_strdup(glob_result.gl_pathv[j]);
                            }
                            globfree(&glob_result);
                        } else {
                            items = mem_stack_realloc_array(items, item_count * sizeof(char*), (item_count + 1) * sizeof(char *), 1);
                            items[item_count++] = expanded;
                            expanded = NULL;
                        }
                        // No free needed for pattern
                    } else {
                        items = mem_stack_realloc_array(items, item_count * sizeof(char*), (item_count + 1) * sizeof(char *), 1);
                        items[item_count++] = expanded;
                        expanded = NULL;
                    }
                    // No free needed for expanded
                }
                // No free needed for expanded_list
            }
        }
    } else {
        char **args = posish_var_get_all_positional();
        if (args) {
            for (int i = 0; args[i] != NULL; i++) {
                items = mem_stack_realloc_array(items, item_count * sizeof(char*), (item_count + 1) * sizeof(char *), 1);
                items[item_count++] = mem_stack_strdup(args[i]);
                free(args[i]);
            }
            free(args);
        }
    }
    
    int status = 0;
    for (size_t i = 0; i < item_count; i++) {
        struct stackmark smark;
        mem_stack_push_mark(&smark);
        
        // Check for Ctrl+C interruption
        if (signal_check_sigint()) {
            fprintf(stderr, "\n");
            mem_stack_pop_mark(&smark);
            return 130;  // 128 + SIGINT
        }
        
        if (posish_var_set(node->data.for_loop.var_name, items[i]) != 0) {
            // Assignment failed (readonly variable)
            status = 1;
            mem_stack_pop_mark(&smark);
            break;
        }
        if (node->data.for_loop.body) {
            status = executor_execute(node->data.for_loop.body);
            if (status == EXIT_BREAK) {
                if (executor_break_count > 1) {
                    executor_break_count--;
                    mem_stack_pop_mark(&smark);
                    return EXIT_BREAK;
                }
                status = 0;
                mem_stack_pop_mark(&smark);
                break;
            } else if (status == EXIT_CONTINUE) {
                if (executor_continue_count > 1) {
                    executor_continue_count--;
                    mem_stack_pop_mark(&smark);
                    return EXIT_CONTINUE;
                }
                status = 0;
                mem_stack_pop_mark(&smark);
                continue;
            } else if (status == EXIT_RETURN) {
                mem_stack_pop_mark(&smark);
                break;
            }
        }
        mem_stack_pop_mark(&smark);
        // free(items[i]); // No free needed
    }
    // if (items) free(items); // No free needed
    
    return status;
}

static int execute_if(ASTNode *node) {
    int old_ignore = shell_ignore_errexit;
    shell_ignore_errexit = 1;
    int status = executor_execute(node->data.if_stmt.condition);
    shell_ignore_errexit = old_ignore;
    
    if (status == 0) {
        return executor_execute(node->data.if_stmt.then_branch);
    } else {
        if (node->data.if_stmt.else_branch) {
            return executor_execute(node->data.if_stmt.else_branch);
        }
    }
    return 0;
}

static int execute_while(ASTNode *node) {
    int status = 0;
    while (1) {
        struct stackmark smark;
        mem_stack_push_mark(&smark);

        // Check for Ctrl+C interruption
        if (signal_check_sigint()) {
            fprintf(stderr, "\n");
            mem_stack_pop_mark(&smark);
            return 130;  // 128 + SIGINT
        }

        int old_ignore = shell_ignore_errexit;
        shell_ignore_errexit = 1;
        int cond_status = executor_execute(node->data.while_loop.condition);
        shell_ignore_errexit = old_ignore;
        
        if (cond_status != 0) {
            mem_stack_pop_mark(&smark);
            break;
        }
        
        status = executor_execute(node->data.while_loop.body);
        if (status == EXIT_BREAK) {
            if (executor_break_count > 1) {
                executor_break_count--;
                mem_stack_pop_mark(&smark);
                return EXIT_BREAK;
            }
            status = 0;
            mem_stack_pop_mark(&smark);
            break;
        } else if (status == EXIT_CONTINUE) {
            if (executor_continue_count > 1) {
                executor_continue_count--;
                mem_stack_pop_mark(&smark);
                return EXIT_CONTINUE;
            }
            status = 0;
            mem_stack_pop_mark(&smark);
            continue;
        } else if (status == EXIT_RETURN) {
            mem_stack_pop_mark(&smark);
            return status;
        }
        mem_stack_pop_mark(&smark);
    }
    return status;
}

static int execute_until(ASTNode *node) {
    int status = 0;
    while (1) {
        struct stackmark smark;
        mem_stack_push_mark(&smark);

        // Check for Ctrl+C interruption
        if (signal_check_sigint()) {
            fprintf(stderr, "\n");
            mem_stack_pop_mark(&smark);
            return 130;  // 128 + SIGINT
        }

        int old_ignore = shell_ignore_errexit;
        shell_ignore_errexit = 1;
        int cond_status = executor_execute(node->data.until_loop.condition);
        shell_ignore_errexit = old_ignore;
        
        if (cond_status == 0) {
            mem_stack_pop_mark(&smark);
            break;
        }
        
        status = executor_execute(node->data.until_loop.body);
        if (status == EXIT_BREAK) {
            if (executor_break_count > 1) {
                executor_break_count--;
                mem_stack_pop_mark(&smark);
                return EXIT_BREAK;
            }
            status = 0;
            mem_stack_pop_mark(&smark);
            break;
        } else if (status == EXIT_CONTINUE) {
            if (executor_continue_count > 1) {
                executor_continue_count--;
                mem_stack_pop_mark(&smark);
                return EXIT_CONTINUE;
            }
            status = 0;
            mem_stack_pop_mark(&smark);
            continue;
        } else if (status == EXIT_RETURN) {
            mem_stack_pop_mark(&smark);
            return EXIT_RETURN;
        }
        mem_stack_pop_mark(&smark);
    }
    return status;
}


static int is_safe_for_vfork(ASTNode *node) {
    if (!node) return 1;
    
    switch (node->type) {
        case NODE_COMMAND: {
            // Assignments are unsafe (modify state)
            if (node->data.command.assignment_count > 0) return 0;
            
            // Empty command is safe
            if (node->data.command.arg_count == 0) return 1;
            
            char *cmd = node->data.command.args[0];
            if (!cmd) return 1;
            
            // Check for pure builtins that don't modify shell state
            if (strcmp(cmd, ":") == 0 ||
                strcmp(cmd, "true") == 0 ||
                strcmp(cmd, "false") == 0 ||
                strcmp(cmd, "echo") == 0 ||
                strcmp(cmd, "printf") == 0 ||
                strcmp(cmd, "test") == 0 ||
                strcmp(cmd, "[") == 0) {
                return 1;
            }
            
            // Check if it's a builtin at all (all others are potentially unsafe)
            if (builtin_is_builtin(cmd)) {
                return 0;
            }
            
            // Check if it's a function (we don't analyze them, so assume unsafe)
            if (func_get(cmd)) {
                return 0;
            }
            
            // External command - SAFE! (execve replaces memory)
            return 1;
        }
        case NODE_PIPELINE:
        case NODE_AND:
        case NODE_OR:
            return is_safe_for_vfork(node->data.pipeline.left) && 
                   is_safe_for_vfork(node->data.pipeline.right);
                   
        case NODE_LIST:
            return is_safe_for_vfork(node->data.list.left) && 
                   is_safe_for_vfork(node->data.list.right);
                   
        case NODE_IF:
            return is_safe_for_vfork(node->data.if_stmt.condition) &&
                   is_safe_for_vfork(node->data.if_stmt.then_branch) &&
                   is_safe_for_vfork(node->data.if_stmt.else_branch);
                   
        case NODE_SUBSHELL:
            return is_safe_for_vfork(node->data.subshell.body);
            
        case NODE_GROUP:
            return is_safe_for_vfork(node->data.group.body);
            
        // Loops, cases, functions, etc. are too complex/unsafe
        default:
            return 0;
    }
}

static int execute_subshell(ASTNode *node) {
    // Use vfork() if safe (no state modification), otherwise fork()
    pid_t pid;
    if (is_safe_for_vfork(node->data.subshell.body)) {
        pid = vfork();
    } else {
        pid = fork();
    }
    
    if (pid == 0) {
        // Child process
        int status = executor_execute(node->data.subshell.body);
        exit(status);  // Use exit() (or _exit)
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return 1;
    } else {
        perror("fork");
        return 1;
    }
}

static int execute_case(ASTNode *node) {
    char *word = expand_word(node->data.case_stmt.word);
    int status = 0;
    int matched = 0;

    for (size_t i = 0; i < node->data.case_stmt.item_count; i++) {
        CaseItem *item = &node->data.case_stmt.items[i];
        
        if (item->patterns) {
            for (int j = 0; item->patterns[j]; j++) {
                char *pattern = expand_word(item->patterns[j]);
                
                if (fnmatch(pattern, word, 0) == 0) {
                    matched = 1;
                    // free(pattern); // No free needed
                    break;
                }
                // free(pattern); // No free needed
            }
        }
        
        if (matched) {
            if (item->commands) {
                status = executor_execute(item->commands);
            }
            break;
        }
    }
    
    // free(word); // No free needed
    return status;
}

static int execute_group(ASTNode *node) {
    return executor_execute(node->data.group.body);
}

static int execute_function_def(ASTNode *node) {
    ASTNode *body_copy = ast_clone_to_heap(node->data.function.body);
    func_add(node->data.function.name, body_copy);
    return 0;
}

static int execute_and_or(ASTNode *node) {
    int old_ignore = shell_ignore_errexit;
    shell_ignore_errexit = 1;
    int status = executor_execute(node->data.pipeline.left);
    shell_ignore_errexit = old_ignore;
    
    if (node->type == NODE_AND) {
        if (status == 0) {
            return executor_execute(node->data.pipeline.right);
        } else {
            return status;
        }
    } else if (node->type == NODE_OR) {
        if (status != 0) {
            return executor_execute(node->data.pipeline.right);
        } else {
            return status;
        }
    }
    return status;
}

int executor_execute(ASTNode *node) {
    if (!node) return 0;

    signal_check_pending();

    // Update LINENO
    if (node->lineno > 0) {
        posish_var_set_lineno(node->lineno);
    }

    int status = 0;
    if (node->type == NODE_COMMAND) {
        status = execute_simple_command(node);
    } else if (node->type == NODE_PIPELINE) {
        status = execute_pipeline(node);
    } else if (node->type == NODE_LIST) {
        status = execute_list(node);
    } else if (node->type == NODE_IF) {
        status = execute_if(node);
    } else if (node->type == NODE_WHILE) {
        status = execute_while(node);
    } else if (node->type == NODE_UNTIL) {
        status = execute_until(node);
    } else if (node->type == NODE_FOR) {
        status = execute_for(node);
    } else if (node->type == NODE_SUBSHELL) {
        status = execute_subshell(node);
    } else if (node->type == NODE_CASE) {
        status = execute_case(node);
    } else if (node->type == NODE_GROUP) {
        status = execute_group(node);
    } else if (node->type == NODE_FUNCTION) {
        status = execute_function_def(node);
    } else if (node->type == NODE_AND || node->type == NODE_OR) {
        status = execute_and_or(node);
    }

    last_exit_status = status;

    if (shell_exit_on_error && status != 0) {
        // Check if we should ignore error
        // -e is ignored if:
        // 1. command is part of while/until condition
        // 2. command is part of if/elif condition
        // 3. command is part of && or || (except the last one)
        // 4. command is part of ! pipeline
        
        // This requires passing context down to executor_execute.
        // For now, we'll implement a basic version and refine.
        // But wait, POSIX says:
        // "The -e setting shall be ignored when executing the compound list following the while, until, if, or elif reserved word, a pipeline beginning with the ! reserved word, or any command of an AND-OR list other than the last."
        
        // We need a way to know if we are in such a context.
        // We can add a flag to executor_execute or use a global.
        // Let's add a global 'shell_ignore_errexit' in shell_options.
        
        // For now, let's just exit if not ignored.
        // But we haven't implemented the ignore logic yet.
        // If we just exit, we might break scripts that rely on checking status.
        // shellbench uses 'if ...' heavily.
        
        // We MUST implement the ignore logic.
        // I'll add 'shell_ignore_errexit' to shell_options.h/c and set it in execute_if, execute_while, etc.
        
        extern int shell_ignore_errexit;
        if (!shell_ignore_errexit) {
             exit(status);
        }
    }
    return status;
}
// Pattern removal helper functions
// Remove shortest matching suffix
static char *remove_suffix_shortest(const char *str, const char *pattern) {
    if (!str || !pattern) return xstrdup(str ? str : "");
    
    size_t len = strlen(str);
    // Try matching from end backwards (shortest match first)
    for (size_t i = len; i > 0; i--) {
        if (fnmatch(pattern, str + i, 0) == 0) {
            char *result = xmalloc(i + 1);
            strncpy(result, str, i);
            result[i] = '\0';
            return result;
        }
    }
    return xstrdup(str);
}

// Remove longest matching suffix
static char *remove_suffix_longest(const char *str, const char *pattern) {
    if (!str || !pattern) return xstrdup(str ? str : "");
    
    size_t len = strlen(str);
    // Try matching from start forwards (longest match first)
    for (size_t i = 0; i <= len; i++) {
        if (fnmatch(pattern, str + i, 0) == 0) {
            char *result = xmalloc(i + 1);
            strncpy(result, str, i);
            result[i] = '\0';
            return result;
        }
    }
    return xstrdup(str);
}

// Remove shortest matching prefix
static char *remove_prefix_shortest(const char *str, const char *pattern) {
    if (!str || !pattern) return xstrdup(str ? str : "");
    
    size_t len = strlen(str);
    // Try matching from start (shortest match first)
    for (size_t i = 0; i <= len; i++) {
        char temp[i + 1];
        strncpy(temp, str, i);
        temp[i] = '\0';
        if (fnmatch(pattern, temp, 0) == 0) {
            return xstrdup(str + i);
        }
    }
    return xstrdup(str);
}

// Remove longest matching prefix
static char *remove_prefix_longest(const char *str, const char *pattern) {
    if (!str || !pattern) return xstrdup(str ? str : "");
    
    size_t len = strlen(str);
    // Try matching from end backwards (longest match first)
    for (size_t i = len; i > 0; i--) {
        char temp[i + 1];
        strncpy(temp, str, i);
        temp[i] = '\0';
        if (fnmatch(pattern, temp, 0) == 0) {
            return xstrdup(str + i);
        }
    }
    return xstrdup(str);
}
