/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <pwd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h> // Added
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "variables.h"
#include "jobs.h"
#include "line_editor.h"
#include "error.h" // Added
#include "memalloc.h" // Added
#include "input.h" // Added
#include "signals.h" // Added
#include "shell_options.h" // Added
#include "mem_stack.h" // Added

#define MAX_LINE 1024

extern char **environ;

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_update_status(pid, JOB_DONE);
            // Ideally remove it, but we might want to show "Done" once.
            // For now, let's just update.
        } else if (WIFSTOPPED(status)) {
            job_update_status(pid, JOB_STOPPED);
        } else if (WIFCONTINUED(status)) {
            job_update_status(pid, JOB_RUNNING);
        }
    }
}

char *expand_prompt(const char *ps1) {
    if (!ps1) return strdup("posish$ ");
    
    char *buffer = malloc(MAX_LINE);
    if (!buffer) return NULL;
    
    size_t i = 0, j = 0;
    while (ps1[i] && j < MAX_LINE - 1) {
        if (ps1[i] == '\\') {
            i++;
            if (!ps1[i]) break;
            
            if (ps1[i] == 'u') {
                // Username
                uid_t uid = getuid();
                struct passwd *pw = getpwuid(uid);
                if (pw) {
                    const char *user = pw->pw_name;
                    size_t len = strlen(user);
                    if (j + len < MAX_LINE) {
                        strcpy(buffer + j, user);
                        j += len;
                    }
                }
            } else if (ps1[i] == 'h') {
                // Hostname
                char hostname[HOST_NAME_MAX + 1];
                if (gethostname(hostname, sizeof(hostname)) == 0) {
                    // Short hostname (up to first dot)
                    char *dot = strchr(hostname, '.');
                    if (dot) *dot = '\0';
                    size_t len = strlen(hostname);
                    if (j + len < MAX_LINE) {
                        strcpy(buffer + j, hostname);
                        j += len;
                    }
                }
            } else if (ps1[i] == 'w') {
                // Working directory
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd))) {
                    // Replace home with ~
                    const char *home = getenv("HOME");
                    if (home) {
                        size_t home_len = strlen(home);
                        if (strncmp(cwd, home, home_len) == 0) {
                            if (j + 1 < MAX_LINE) {
                                buffer[j++] = '~';
                                size_t len = strlen(cwd + home_len);
                                if (j + len < MAX_LINE) {
                                    strcpy(buffer + j, cwd + home_len);
                                    j += len;
                                }
                                i++;
                                continue;
                            }
                        }
                    }
                    size_t len = strlen(cwd);
                    if (j + len < MAX_LINE) {
                        strcpy(buffer + j, cwd);
                        j += len;
                    }
                }
            } else if (ps1[i] == '$') {
                // $ or #
                if (j + 1 < MAX_LINE) {
                    buffer[j++] = (geteuid() == 0) ? '#' : '$';
                }
            } else {
                // Literal
                if (j + 1 < MAX_LINE) {
                    buffer[j++] = ps1[i];
                }
            }
        } else {
            buffer[j++] = ps1[i];
        }
        i++;
    }
    buffer[j] = '\0';
    return buffer;
}

void print_prompt() {
    if (isatty(STDIN_FILENO)) {
        printf("posish$ ");
        fflush(stdout);
    }
}

// Forward declaration
static int try_fast_command(const char *cmd);



static int run_script_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0; // Silent fail for startup files? Or return error?
    // For startup files we usually ignore if not found, but here we pass existing files.
    // If called from main for script arg, we should error.
    // Let's make it return status.
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return 1;
    }

    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';
    fclose(file);

    Lexer lexer;
    lexer_init(&lexer, content);
    
    // Fast-path: Skip parser for simple commands
    if (try_fast_command(content)) {
        free(content);
        return 0;
    }
    
    struct stackmark smark;
    mem_stack_push_mark(&smark);
    
    ASTNode *ast = parser_parse(&lexer);
    
    int status = 0;
    if (ast) {
        status = executor_execute(ast);
        ast_free(ast);
    }
    
    mem_stack_pop_mark(&smark);
    
    free(content);
    return status;
}

// Fast-path handler - returns 1 if handled, 0 if needs full parse
static int try_fast_command(const char *cmd) {
    // Skip whitespace
    while (*cmd && (*cmd == ' ' || *cmd == '\t')) cmd++;
    if (!*cmd) return 1;
    if (*cmd == '#') {
        // If it's a comment, check if there's a newline.
        // If there's a newline, we must parse the rest.
        // If no newline, it's just a comment line, so we are done.
        if (strchr(cmd, '\n')) return 0; // Let parser handle multi-line
        return 1; // Just a comment
    }
    
    // Fast-path: Simple assignment (VAR=value)
    // But reject if there's a semicolon (compound command)
    if (strchr(cmd, ';')) return 0; // Not simple, use full parser
    
    const char *eq = strchr(cmd, '=');
    if (eq && eq > cmd) {
        const char *p = cmd;
        while (p < eq && (isalnum(*p) || *p == '_')) p++;
        
        if (p == eq) {
            char *name = strndup(cmd, eq - cmd);
            const char *val_start = eq + 1;
            while (*val_start == ' ' || *val_start == '\t') val_start++;
            size_t val_len = strlen(val_start);
            while (val_len > 0 && (val_start[val_len-1] == ' ' || val_start[val_len-1] == '\t' || val_start[val_len-1] == '\n'))
                val_len--;
            char *value = strndup(val_start, val_len);
            
            var_set(name, value);
            free(name);
            free(value);
            return 1;
        }
    }
    
    // Fast-path: Colon command
    if (cmd[0] == ':' && (cmd[1] == '\0' || cmd[1] == ' ' || cmd[1] == '\t' || cmd[1] == '\n'))
        return 1;
    
    return 0;
}

#include "signals.h"

int main(int argc, char **argv) {
    // Initialize variables from environment
    var_init(environ);
    job_init();
    signal_init();
    var_set_shell_name(argv[0]);
    
    int is_login_shell = (argv[0][0] == '-');
    int force_interactive = 0;
    int read_from_stdin = 0;
    const char *command_string = NULL;
    const char *command_name = NULL;
    int arg_idx = 1;
    
    // Check for --login flag
    if (argc > 1 && strcmp(argv[1], "--login") == 0) {
        is_login_shell = 1;
        arg_idx++;
    }
    
    // Parse options following POSIX sh spec
    while (arg_idx < argc && argv[arg_idx][0] == '-' && argv[arg_idx][1] != '\0') {
        // "--" terminates option processing
        if (strcmp(argv[arg_idx], "--") == 0) {
            arg_idx++;
            break;
        }
        
        // Single "-" means read from stdin and treat remaining args as positional
        if (strcmp(argv[arg_idx], "-") == 0) {
            read_from_stdin = 1;
            arg_idx++;
            break;
        }
        
        const char *opts = &argv[arg_idx][1];
        
        // Handle each option character
        for (const char *p = opts; *p; p++) {
            switch (*p) {
                case 'c':
                    // -c requires next arg to be command string
                    if (*(p+1) != '\0') {
                        fprintf(stderr, "%s: -c: option cannot be combined with others\n", argv[0]);
                        return 2;
                    }
                    arg_idx++;
                    if (arg_idx >= argc) {
                        fprintf(stderr, "%s: -c: option requires an argument\n", argv[0]);
                        return 2;
                    }
                    command_string = argv[arg_idx];
                    // Next arg (if present) is command_name ($0)
                    arg_idx++;
                    if (arg_idx < argc) {
                        command_name = argv[arg_idx];
                        arg_idx++;
                    }
                    goto done_parsing_options;
                    
                case 'i':
                    force_interactive = 1;
                    break;
                    
                case 's':
                    read_from_stdin = 1;
                    break;
                    
                case 'x':  // trace mode
                    shell_trace_mode = 1;
                    break;
                    
                // Note: Other set options (e,f,v,n,u,a,m,b,C,h) would go here
                // For now we'll just accept and ignore them to avoid errors
                case 'e': case 'f': case 'v': case 'n': case 'u':
                case 'a': case 'm': case 'b': case 'C': case 'h':
                    // Accepted but not yet implemented
                    break;
                    
                default:
                    fprintf(stderr, "%s: -%c: invalid option\n", argv[0], *p);
                    return 2;
            }
        }
        arg_idx++;
    }
    
done_parsing_options:
    
    // Handle -c option (command string)
    if (command_string) {
        // Set positional parameters from remaining args
        int param_count = 0;
        if (arg_idx < argc) {
            param_count = argc - arg_idx;
            var_set_positional(param_count, &argv[arg_idx]);
        }
        
        // Set $0
        if (command_name) {
            var_set("0", command_name);
        }
        
        // If login shell, source profiles first
        if (is_login_shell) {
            if (access("/etc/profile", R_OK) == 0) run_script_file("/etc/profile");
            const char *home = getenv("HOME");
            if (home) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/.profile", home);
                if (access(path, R_OK) == 0) run_script_file(path);
            }
        }
        
        // Execute command string
        // FAST-PATH: Skip parser for simple commands
        if (try_fast_command(command_string)) {
            return 0;
        }
        
        // Full parse path
        Lexer lexer;
        lexer_init(&lexer, command_string);
        
        struct stackmark smark;
        mem_stack_push_mark(&smark);
        
        ASTNode *ast = parser_parse(&lexer);
        
        int status = 0;
        if (ast) {
            status = executor_execute(ast);
            ast_free(ast);
        } else {
            fprintf(stderr, "%s: parse error\n", argv[0]);
            mem_stack_pop_mark(&smark);
            return 2;
        }
        
        mem_stack_pop_mark(&smark);
        
        return status;
    }

    // Handle script file
    if (arg_idx < argc && !read_from_stdin) {
        const char *filename = argv[arg_idx];
        // Set positional parameters from remaining args (skip script filename)
        arg_idx++;
        if (arg_idx < argc) {
            int param_count = argc - arg_idx;
            var_set_positional(param_count, &argv[arg_idx]);
        }
        
        if (access(filename, R_OK) != 0) {
             fprintf(stderr, "%s: %s: No such file or directory\n", argv[0], filename);
             return 127;
        }
        return run_script_file(filename);
    }

    // Determine if this is an interactive shell
    // Interactive if: -i flag OR (stdin is terminal AND no -c or script file)
    int is_interactive = force_interactive || 
                         (isatty(STDIN_FILENO) && isatty(STDERR_FILENO) && 
                          !command_string && arg_idx >= argc);

    // Interactive or Login Shell
    if (is_login_shell) {
        // Source /etc/profile
        if (access("/etc/profile", R_OK) == 0) {
            run_script_file("/etc/profile");
        }
        
        // Source ~/.profile
        const char *home = getenv("HOME");
        if (home) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/.profile", home);
            if (access(path, R_OK) == 0) {
                run_script_file(path);
            }
        }
    }
    
    // For interactive shells (login or not), we might need ENV?
    // POSIX says: "If the shell is interactive, it shall expand the value of the ENV variable... and source it."
    // Wait, usually ENV is for interactive shells.
    // Bash man page: "When bash is invoked as an interactive shell that is not a login shell, it reads and executes commands from ~/.bashrc"
    // POSIX sh: "If the shell is interactive, it shall expand the value of the parameter ENV... and execute the file."
    // So it applies to ALL interactive shells?
    // Usually login shells source profile, non-login source ENV/rc.
    // But POSIX says ENV is for interactive invocation.
    // Let's follow POSIX: If interactive, source ENV.
    
    if (is_interactive) {
        char *env_var = var_get("ENV");
        if (env_var && *env_var) {
            // Expand variable if it contains expansions? POSIX says "parameter expansion".
            // For simplicity, we'll just handle direct path or tilde.
            // A full implementation would run the expansion logic.
            // Let's assume it's a path for now.
            
            char *expanded_path = NULL;
            if (env_var[0] == '~') {
                const char *home = getenv("HOME");
                if (home) {
                    size_t len = strlen(home) + strlen(env_var);
                    expanded_path = malloc(len + 1);
                    sprintf(expanded_path, "%s%s", home, env_var + 1);
                }
            } else {
                expanded_path = strdup(env_var);
            }
            
            if (expanded_path) {
                if (access(expanded_path, R_OK) == 0) {
                    run_script_file(expanded_path);
                }
                free(expanded_path);
            }
            free(env_var);
        }
    }

    if (is_interactive) {
        // Interactive mode setup
        
        // Put shell in its own process group
        while (tcgetpgrp(STDIN_FILENO) != (getpgrp())) {
            kill(-getpgrp(), SIGTTIN);
        }
        
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        
        pid_t pid = getpid();
        setpgid(pid, pid);
        tcsetpgrp(STDIN_FILENO, pid);
        
        // Initialize history
        const char *home = getenv("HOME");
        if (home) {
            char history_path[1024];
            snprintf(history_path, sizeof(history_path), "%s/.sh_history", home);
            history_init(history_path);
        }
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    // REPL Loop
    char *command_buffer = NULL;

    while (1) {
        signal_check_pending();
        char *prompt_str;
        if (command_buffer) {
            const char *ps2 = var_get("PS2");
            if (!ps2) ps2 = "> ";
            prompt_str = strdup(ps2);
        } else {
            const char *ps1 = var_get("PS1");
            if (!ps1) ps1 = "posish$ "; // Default
            prompt_str = expand_prompt(ps1);
        }
        
        char *line = read_line(prompt_str);
        free(prompt_str);
        
        if (!line) {
            if (command_buffer) {
                // EOF during incomplete command
                fprintf(stderr, "\n%s: syntax error: unexpected end of file\n", argv[0]);
                free(command_buffer);
                command_buffer = NULL;
                // Reset state? For now just continue to next prompt (which will exit loop if EOF persists)
                // Actually read_line returning NULL usually means Ctrl-D.
                // If we are in continuation, we should abort this command but stay in shell?
                // But read_line returns NULL on EOF. If we loop, we might get NULL again immediately.
                // Let's just break for now, or handle it like bash (exit shell).
                // Bash exits shell on EOF even in continuation if it's the only thing.
                // But if I type "echo 'hi<Ctrl-D>", bash prints error and stays.
                // Our read_line might behave differently.
                // Let's assume NULL means stream closed.
                break; 
            }
            break; // EOF
        }

        // Append line to buffer
        if (command_buffer) {
            size_t old_len = strlen(command_buffer);
            size_t new_len = strlen(line);
            command_buffer = realloc(command_buffer, old_len + new_len + 1);
            strcat(command_buffer, line);
            free(line);
        } else {
            command_buffer = line;
        }

        // Check if complete
        if (lexer_check_incomplete(command_buffer) == 0) {
            // Complete!
            Lexer lexer;
            lexer_init(&lexer, command_buffer);
            
            struct stackmark smark;
            mem_stack_push_mark(&smark);
            
            ASTNode *ast = parser_parse(&lexer);

            if (ast) {
                history_add(command_buffer);
                executor_execute(ast);
                ast_free(ast);
            }
            
            mem_stack_pop_mark(&smark);
            
            free(command_buffer);
            command_buffer = NULL;
        }
        // Else continue loop to read more
    }

    return 0;
}
