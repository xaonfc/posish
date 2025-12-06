/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "error.h"
#include "alias.h"
#include "variables.h"
#include "memalloc.h"
#include "memalloc.h"  // TODO: remove duplicate
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

// Forward declarations
typedef struct {
    Lexer *lexer;
    Token current_token;
    int has_token;
} Parser;

// Fast-path handler - returns 1 if handled, 0 if needs full parse
int parser_try_fast_path(const char *cmd) {
    // Skip whitespace
    while (*cmd && (*cmd == ' ' || *cmd == '\t')) cmd++;
    if (!*cmd) return 1; // Empty line handled
    
    // Check for blank line or comment
    if (*cmd == '#' || *cmd == '\n') {
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
            // Check if var name is valid (starts with alpha/underscore)
            if (!isalpha(cmd[0]) && cmd[0] != '_') return 0;
            
            // Check value has no special shell characters that need expansion
            int is_simple = 1;
            for (const char *c = eq + 1; *c && is_simple; c++) {
                if (*c == '$' || *c == '`' || *c == '\\' || *c == '"' || *c == '\'' || 
                    *c == '*' || *c == '?' || *c == '[' || *c == '~' || *c == '\n') {
                    is_simple = 0;
                }
            }
            
            if (is_simple) {
                // Fast assignment path
                size_t name_len = eq - cmd;
                char *name = mem_stack_alloc(name_len + 1);
                strncpy(name, cmd, name_len);
                name[name_len] = '\0';
                
                // Find end of value (trim trailing whitespace/newline)
                const char *val_start = eq + 1;
                const char *val_end = val_start + strlen(val_start);
                while (val_end > val_start && (val_end[-1] == ' ' || val_end[-1] == '\t' || val_end[-1] == '\n')) {
                    val_end--;
                }
                
                size_t val_len = val_end - val_start;
                char *value = mem_stack_alloc(val_len + 1);
                strncpy(value, val_start, val_len);
                value[val_len] = '\0';
                
                posish_var_set(name, value);
                // Success status handled normally (return 0 usually)
                return 1;
            }
        }
    }
    
    // Fast-path: Colon command
    if (cmd[0] == ':' && (cmd[1] == '\0' || cmd[1] == ' ' || cmd[1] == '\t' || cmd[1] == '\n'))
        return 1;
    
    return 0;
}

static ASTNode *parse_pipeline(Parser *parser);
static ASTNode *parse_simple_command(Parser *parser);

// Helper to peek at the next token without consuming it (requires lexer support or lookahead buffer)
// For now, we'll just consume and if it's not what we want, we might be in trouble.
// But our grammar is simple enough.



static Token parser_peek(Parser *parser) {
    if (!parser->has_token) {
        parser->current_token = lexer_next_token(parser->lexer);
        parser->has_token = 1;
    }
    return parser->current_token;
}

static Token parser_consume(Parser *parser) {
    Token token = parser_peek(parser);
    parser->has_token = 0; // We consumed it, so next peek will fetch new
    return token;
}

static ASTNode *parse_list(Parser *parser);

ASTNode *parser_parse(Lexer *lexer) {
    Parser parser = {lexer, {0, NULL, 0}, 0};
    
    // Parse a list (top level)
    ASTNode *node = parse_list(&parser);
    
    // Check for unexpected tokens left over
    if (parser.has_token) {
        Token token = parser.current_token;
        if (token.type != TOKEN_EOF && token.type != TOKEN_NEWLINE) {
            // If we have a node, we might have parsed "cmd; fi"
            // If we don't have a node, we might have parsed "fi"
            
            // If it's a keyword that terminates a block, it's unexpected here
            if (token.type == TOKEN_KEYWORD) {
                if (strcmp(token.value, "then") == 0 || strcmp(token.value, "else") == 0 || 
                    strcmp(token.value, "fi") == 0 || strcmp(token.value, "do") == 0 || 
                    strcmp(token.value, "done") == 0 || strcmp(token.value, "esac") == 0 ||
                    strcmp(token.value, "}") == 0) {
                        
                    char *shell_name = posish_var_get_shell_name();
                    fprintf(stderr, "%s: syntax error near unexpected token `%s'\n", 
                            shell_name ? shell_name : "posish", token.value);
                    if (shell_name) free(shell_name);
                    if (node) ast_free(node);
                    free_token(token);
                    return NULL;
                }
            }
            // Also check for unexpected operators like ';;' or ')'
            if (token.type == TOKEN_OPERATOR) {
                if (strcmp(token.value, ";;") == 0 || strcmp(token.value, ")") == 0) {
                    char *shell_name = posish_var_get_shell_name();
                    fprintf(stderr, "%s: syntax error near unexpected token `%s'\n",
                            shell_name ? shell_name : "posish", token.value);
                    if (shell_name) free(shell_name);
                    if (node) ast_free(node);
                    free_token(token);
                    return NULL;
                }
            }
        }
        free_token(parser.current_token);
    }
    
    // If parse_list returns NULL (empty input), return empty command
    if (!node) {
        node = ast_new_command(); // Empty command is a no-op
    }
    
    return node;
}
static ASTNode *parse_compound_list(Parser *parser, const char *terminator);

static ASTNode *parse_if_tail(Parser *parser);

static ASTNode *parse_if_statement(Parser *parser) {
    Token token = parser_consume(parser);
    int lineno = token.lineno;
    free_token(token);
    
    ASTNode *node = parse_if_tail(parser);
    if (node) node->lineno = lineno;
    return node;
}

static ASTNode *parse_if_tail(Parser *parser) {
    ASTNode *condition = parse_compound_list(parser, "then");
    if (!condition) return NULL;
    
    Token token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "then") != 0) {
        ast_free(condition);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    ASTNode *then_branch = parse_compound_list(parser, "else"); 
    
    token = parser_peek(parser);
    ASTNode *else_branch = NULL;
    
    if (token.type == TOKEN_KEYWORD) {
        if (strcmp(token.value, "elif") == 0) {
            token = parser_consume(parser);
            int lineno = token.lineno;
            free_token(token);
            
            else_branch = parse_if_tail(parser);
            if (else_branch) else_branch->lineno = lineno;
            else {
                ast_free(condition);
                if (then_branch) ast_free(then_branch);
                return NULL;
            }
        } else if (strcmp(token.value, "else") == 0) {
            token = parser_consume(parser);
            free_token(token);
            
            else_branch = parse_compound_list(parser, "fi");
            
            token = parser_peek(parser);
            if (token.type != TOKEN_KEYWORD || strcmp(token.value, "fi") != 0) {
                ast_free(condition);
                if (then_branch) ast_free(then_branch);
                if (else_branch) ast_free(else_branch);
                return NULL;
            }
            token = parser_consume(parser);
            free_token(token);
        } else if (strcmp(token.value, "fi") == 0) {
            token = parser_consume(parser);
            free_token(token);
        } else {
            // Error
            ast_free(condition);
            if (then_branch) ast_free(then_branch);
            return NULL;
        }
    } else {
        // Error
        ast_free(condition);
        if (then_branch) ast_free(then_branch);
        return NULL;
    }
    
    return ast_new_if(condition, then_branch, else_branch);
}

static ASTNode *parse_while_loop(Parser *parser);
static int parse_redirection(Parser *parser, ASTNode *cmd);
static ASTNode *parse_while_loop(Parser *parser) {
    // Expect 'while'
    Token token = parser_consume(parser);
    int lineno = token.lineno;
    free_token(token);
    
    ASTNode *condition = parse_compound_list(parser, "do");
    if (!condition) return NULL;
    
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "do") != 0) {
        ast_free(condition);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    ASTNode *body = parse_compound_list(parser, "done");
    
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "done") != 0) {
        ast_free(condition);
        if (body) ast_free(body);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    ASTNode *node = ast_new_while(condition, body);
    node->lineno = lineno;
    return node;
}

static ASTNode *parse_until_loop(Parser *parser) {
    // Expect 'until'
    Token token = parser_consume(parser);
    free_token(token);
    
    ASTNode *condition = parse_compound_list(parser, "do");
    if (!condition) return NULL;
    
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "do") != 0) {
        ast_free(condition);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    ASTNode *body = parse_compound_list(parser, "done");
    
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "done") != 0) {
        ast_free(condition);
        if (body) ast_free(body);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    return ast_new_until(condition, body);
}

static ASTNode *parse_for_loop(Parser *parser) {
    // Expect 'for'
    Token token = parser_consume(parser);
    int lineno = token.lineno;
    free_token(token);
    
    // Expect variable name
    token = parser_peek(parser);
    if (token.type != TOKEN_WORD) {
        return NULL;
    }
    char *var_name = mem_stack_strdup(token.value); // Copy before freeing
    token = parser_consume(parser);
    free_token(token);
    
    // Check for 'in' or ';' or 'do'
    token = parser_peek(parser);
    
    char **word_list = NULL;
    size_t word_count = 0;
    
    if (token.type == TOKEN_KEYWORD && strcmp(token.value, "in") == 0) {
        // for name in word...; do
        token = parser_consume(parser);
        free_token(token);
        
        // Collect words until we hit ';' or 'do'
        while (1) {
            token = parser_peek(parser);
            if (token.type == TOKEN_EOF || token.type == TOKEN_NEWLINE) {
                break;
            }
            if (token.type == TOKEN_OPERATOR && strcmp(token.value, ";") == 0) {
                break;
            }
            if (token.type == TOKEN_KEYWORD && strcmp(token.value, "do") == 0) {
                break;
            }
            
            // Add word to list
            word_list = mem_stack_realloc_array(word_list, word_count, word_count + 1, sizeof(char*));
            word_list[word_count++] = mem_stack_strdup(token.value);
            token = parser_consume(parser);
            free_token(token);
        }
        
        // Skip optional ';' or newline
        token = parser_peek(parser);
        if (token.type == TOKEN_OPERATOR && strcmp(token.value, ";") == 0) {
            token = parser_consume(parser);
            free_token(token);
        } else if (token.type == TOKEN_NEWLINE) {
            token = parser_consume(parser);
            free_token(token);
        }
    }
    // else: for name; do or for name do (iterates over $@)
    
    // Skip optional ';' or newline before 'do'
    token = parser_peek(parser);
    if (token.type == TOKEN_OPERATOR && strcmp(token.value, ";") == 0) {
        token = parser_consume(parser);
        free_token(token);
    } else if (token.type == TOKEN_NEWLINE) {
        token = parser_consume(parser);
        free_token(token);
    }
    
    // Expect 'do'
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "do") != 0) {
        // Stack cleanup handles word_list
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    // Parse body
    ASTNode *body = parse_compound_list(parser, "done");
    
    // Expect 'done'
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "done") != 0) {
        if (body) ast_free(body);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    ASTNode *node = ast_new_for(var_name, word_list, word_count, body);
    node->lineno = lineno;
    return node;
}

static ASTNode *parse_case_statement(Parser *parser) {
    // Expect 'case'
    Token token = parser_consume(parser);
    free_token(token);

    // Expect word
    token = parser_peek(parser);
    if (token.type != TOKEN_WORD) return NULL;
    char *word = mem_stack_strdup(token.value); // Copy before freeing token
    token = parser_consume(parser);
    free_token(token);

    // Expect 'in'
    while (parser_peek(parser).type == TOKEN_NEWLINE) {
        Token nl = parser_consume(parser);
        free_token(nl);
    }
    
    token = parser_consume(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "in") != 0) {
        free_token(token);
        return NULL;
    }
    free_token(token);

    // Parse items
    CaseItem *items = NULL;
    size_t item_count = 0;

    while (1) {
        // Skip newlines
        token = parser_peek(parser);
        while (token.type == TOKEN_NEWLINE) {
            Token t = parser_consume(parser);
            free_token(t);
            token = parser_peek(parser);
        }

        if (token.type == TOKEN_KEYWORD && strcmp(token.value, "esac") == 0) {
            break;
        }
        if (token.type == TOKEN_EOF) break;

        // Parse patterns
        // Optional '('
        if (token.type == TOKEN_OPERATOR && strcmp(token.value, "(") == 0) {
            Token t = parser_consume(parser);
            free_token(t);
            token = parser_peek(parser);
        }

        // Read patterns separated by '|'
        char **patterns = NULL;
        size_t pat_count = 0;
        
        if (token.type != TOKEN_WORD) {
            break; 
        }

        while (1) {
            if (token.type == TOKEN_WORD) {
                patterns = mem_stack_realloc_array(patterns, pat_count, pat_count + 2, sizeof(char*));
                patterns[pat_count++] = mem_stack_strdup(token.value);
                patterns[pat_count] = NULL;
                
                Token t = parser_consume(parser);
                free_token(t);
            }
            
            token = parser_peek(parser);
            if (token.type == TOKEN_OPERATOR && strcmp(token.value, "|") == 0) {
                Token t = parser_consume(parser);
                free_token(t);
                token = parser_peek(parser);
                continue;
            }
            break;
        }

        // Expect ')'
        if (token.type != TOKEN_OPERATOR || strcmp(token.value, ")") != 0) {
            break;
        }
        Token t = parser_consume(parser);
        free_token(t);

        // Parse commands until ';;' or 'esac'
        ASTNode *commands = NULL;
        
        // Skip newlines before commands?
        while (parser_peek(parser).type == TOKEN_NEWLINE) {
             Token nl = parser_consume(parser);
             free_token(nl);
        }
        
        commands = parse_list(parser);
        
        // Add item
        items = mem_stack_realloc_array(items, item_count, item_count + 1, sizeof(CaseItem));
        items[item_count].patterns = patterns;
        items[item_count].commands = commands;
        item_count++;

        // Consume ';;' if present
        token = parser_peek(parser);
        if (token.type == TOKEN_OPERATOR && strcmp(token.value, ";;") == 0) {
            Token t = parser_consume(parser);
            free_token(t);
        }
    }

    // Expect 'esac'
    token = parser_consume(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "esac") != 0) {
        free_token(token);
        return NULL;
    }
    free_token(token);

    return ast_new_case(word, items, item_count);
}

static ASTNode *parse_group_command(Parser *parser) {
    // Expect '{'
    Token token = parser_consume(parser);
    free_token(token);
    
    // Parse body (compound list terminated by '}')
    ASTNode *body = parse_compound_list(parser, "}");
    
    // Expect '}'
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "}") != 0) {
        if (body) ast_free(body);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    return ast_new_group(body);
}

// Helper to parse a list of commands terminated by a keyword
static ASTNode *parse_compound_list(Parser *parser, const char *terminator) {
    ASTNode *head = NULL;
    
    while (1) {
        Token token = parser_peek(parser);
        if (token.type == TOKEN_KEYWORD) {
            if (terminator && strcmp(token.value, terminator) == 0) break;
            if (strcmp(token.value, "else") == 0 || strcmp(token.value, "fi") == 0 || 
                strcmp(token.value, "elif") == 0 ||
                strcmp(token.value, "done") == 0 || strcmp(token.value, "then") == 0 || 
                strcmp(token.value, "do") == 0 || strcmp(token.value, "esac") == 0 ||
                strcmp(token.value, "}") == 0) {
                break;
            }
        }
        if (token.type == TOKEN_EOF) break;
        
        // Skip newlines
        if (token.type == TOKEN_NEWLINE) {
            token = parser_consume(parser);
            free_token(token);
            continue;
        }
        
        ASTNode *node = parse_list(parser);
        if (!node) break;
        
        if (!head) {
            head = node;
        } else {
            head = ast_new_list(head, node, 0);
        }
    }
    return head;
}

static ASTNode *parse_and_or(Parser *parser) {
    ASTNode *left = parse_pipeline(parser);
    if (!left) return NULL;
    
    while (1) {
        Token token = parser_peek(parser);
        if (token.type == TOKEN_OPERATOR) {
            NodeType type;
            if (strcmp(token.value, "&&") == 0) {
                type = NODE_AND;
            } else if (strcmp(token.value, "||") == 0) {
                type = NODE_OR;
            } else {
                break;
            }
            
            parser_consume(parser);
            free_token(token);
            
            // Allow newlines after && or ||
            while (parser_peek(parser).type == TOKEN_NEWLINE) {
                Token nl = parser_consume(parser);
                free_token(nl);
            }
            
            ASTNode *right = parse_pipeline(parser);
            if (!right) {
                // Error: expected command after &&/||
                ast_free(left);
                return NULL;
            }
            
            left = ast_new_binary(type, left, right);
        } else {
            break;
        }
    }
    
    return left;
}

static ASTNode *parse_list(Parser *parser) {
    // Skip newlines
    Token token = parser_peek(parser);
    while (token.type == TOKEN_NEWLINE) {
        parser_consume(parser);
        free_token(token);
        token = parser_peek(parser);
    }

    // Check for keywords that terminate a list (in compound list context)
    if (token.type == TOKEN_KEYWORD) {
        if (strcmp(token.value, "then") == 0 || strcmp(token.value, "else") == 0 || 
            strcmp(token.value, "fi") == 0 || strcmp(token.value, "do") == 0 || 
            strcmp(token.value, "done") == 0 || strcmp(token.value, "esac") == 0 ||
            strcmp(token.value, "}") == 0) {
            return NULL;
        }
    }
    // Check for operators that terminate a list (specifically ;;)
    if (token.type == TOKEN_OPERATOR && strcmp(token.value, ";;") == 0) {
        return NULL;
    }

    ASTNode *left = parse_and_or(parser);
    if (!left) return NULL;
    
    token = parser_peek(parser);
    if (token.type == TOKEN_OPERATOR) {
        if (strcmp(token.value, ";") == 0 || strcmp(token.value, "&") == 0) {
            int async = (strcmp(token.value, "&") == 0);
            parser_consume(parser); // consume separator
            free_token(token);
            
            // Skip newlines
            while (parser_peek(parser).type == TOKEN_NEWLINE) {
                Token nl = parser_consume(parser);
                free_token(nl);
            }
            
            // Check if list ends here (e.g. "cmd;")
            Token next = parser_peek(parser);
            if (next.type == TOKEN_EOF) {
                return ast_new_list(left, NULL, async);
            }
             if (next.type == TOKEN_KEYWORD) {
                if (strcmp(next.value, "then") == 0 || strcmp(next.value, "else") == 0 || 
                    strcmp(next.value, "fi") == 0 || strcmp(next.value, "do") == 0 || 
                    strcmp(next.value, "done") == 0 || strcmp(next.value, "esac") == 0 ||
                    strcmp(next.value, "}") == 0) {
                    return ast_new_list(left, NULL, async);
                }
            }
            if (next.type == TOKEN_OPERATOR && strcmp(next.value, ";;") == 0) {
                return ast_new_list(left, NULL, async);
            }
            
            ASTNode *right = parse_list(parser);
            return ast_new_list(left, right, async);
        }
        
        if (strcmp(token.value, ";;") == 0) {
            return left;
        }
    }
    
    if (token.type == TOKEN_NEWLINE) {
        parser_consume(parser);
        free_token(token);
        
        // Check if next token is terminator
        Token next = parser_peek(parser);
        if (next.type == TOKEN_KEYWORD) {
             if (strcmp(next.value, "then") == 0 || strcmp(next.value, "else") == 0 || 
                strcmp(next.value, "fi") == 0 || strcmp(next.value, "do") == 0 || 
                strcmp(next.value, "done") == 0 || strcmp(next.value, "esac") == 0 ||
                strcmp(next.value, "}") == 0) {
                return left;
            }
        }
        if (next.type == TOKEN_OPERATOR && strcmp(next.value, ";;") == 0) {
            return left;
        }

        ASTNode *right = parse_list(parser);
        if (right) {
            return ast_new_list(left, right, 0);
        }
        return left;
    }
    
    return left;
}

static ASTNode *parse_pipeline(Parser *parser) {
    ASTNode *left = parse_simple_command(parser);
    if (!left) return NULL;
    
    Token token = parser_peek(parser);
    if (token.type == TOKEN_OPERATOR && strcmp(token.value, "|") == 0) {
        parser_consume(parser); // consume '|'
        free_token(token); // free the operator token
        
        ASTNode *right = parse_pipeline(parser); // Recursive for multiple pipes
        if (!right) {
            // Error: expected command after pipe
            ast_free(left);
            return NULL;
        }
        return ast_new_pipeline(left, right);
    }
    
    return left;
}

static ASTNode *parse_function_definition(Parser *parser) {
    // Consumed 'function'
    Token token = parser_consume(parser);
    free_token(token);
    
    // Expect name
    token = parser_peek(parser);
    if (token.type != TOKEN_WORD) return NULL;
    char *name = token.value; // Will be copied by ast_new_function
    token = parser_consume(parser);
    free_token(token);
    
    // Optional parens ()
    token = parser_peek(parser);
    if (token.type == TOKEN_OPERATOR && strcmp(token.value, "(") == 0) {
        token = parser_consume(parser);
        free_token(token);
        token = parser_peek(parser);
        if (token.type == TOKEN_OPERATOR && strcmp(token.value, ")") == 0) {
            token = parser_consume(parser);
            free_token(token);
        } else {
            // Error
            return NULL;
        }
    }
    
    // Parse body
    // Skip newlines
    while (parser_peek(parser).type == TOKEN_NEWLINE) {
        Token nl = parser_consume(parser);
        free_token(nl);
    }
    
    ASTNode *body = parse_simple_command(parser); // Should parse compound command
    if (!body) {
        return NULL;
    }
    
    return ast_new_function(name, body);
}

static int parse_redirection(Parser *parser, ASTNode *cmd);

static ASTNode *parse_simple_command(Parser *parser) {
    Token token = parser_peek(parser);
    
    int is_cmd = 0;
    if (token.type == TOKEN_WORD) is_cmd = 1;
    else if (token.type == TOKEN_OPERATOR && strcmp(token.value, "(") == 0) {
        // Subshell grouping: ( command_list )
        token = parser_consume(parser); // consume '('
        free_token(token);
        
        ASTNode *body = parse_list(parser);
        
        token = parser_peek(parser);
        if (token.type != TOKEN_OPERATOR || strcmp(token.value, ")") != 0) {
            // if (body) ast_free(body); // No-op
            return NULL;
        }
        token = parser_consume(parser); // consume ')'
        free_token(token);
        
        return ast_new_subshell(body);
    }
    else if (token.type == TOKEN_KEYWORD) {
        if (strcmp(token.value, "if") == 0) return parse_if_statement(parser);
        if (strcmp(token.value, "while") == 0) return parse_while_loop(parser);
        if (strcmp(token.value, "until") == 0) return parse_until_loop(parser);
        if (strcmp(token.value, "for") == 0) return parse_for_loop(parser);
        if (strcmp(token.value, "case") == 0) return parse_case_statement(parser);
        if (strcmp(token.value, "{") == 0) return parse_group_command(parser);
        return NULL; 
    }
    else if (token.type == TOKEN_IO_NUMBER) is_cmd = 1;
    else if (token.type == TOKEN_OPERATOR) {
        if (strcmp(token.value, "<") == 0 || strcmp(token.value, ">") == 0 ||
            strcmp(token.value, ">>") == 0 || strcmp(token.value, "<<") == 0 ||
            strcmp(token.value, "<&") == 0 || strcmp(token.value, ">&") == 0 ||
            strcmp(token.value, "<>") == 0 || strcmp(token.value, ">|") == 0) {
            is_cmd = 1;
        }
    }
    
    if (!is_cmd) return NULL;
    
    ASTNode *cmd = ast_new_command();
    cmd->lineno = token.lineno;
    int seen_command_name = 0;
    
    if (token.type == TOKEN_WORD) {
        if (strcmp(token.value, "function") == 0) {
            Token next = parser_peek(parser);
            if (next.type == TOKEN_WORD) {
                return parse_function_definition(parser);
            }
        }

        char *eq = strchr(token.value, '=');
        if (eq && eq != token.value) {
            size_t name_len = eq - token.value;
            char *name_part = strndup(token.value, name_len); // Heap alloc, need free
            int is_valid = posish_var_is_valid_name(name_part);
            free(name_part);
            
            if (is_valid) {
                goto parse_loop;
            }
        }

        Token name_token = parser_consume(parser);
        char *name = mem_stack_strdup(name_token.value);
        free_token(name_token);
        
        Token next = parser_peek(parser);
        if (next.type == TOKEN_OPERATOR && strcmp(next.value, "(") == 0) {
            Token lparen = parser_consume(parser);
            free_token(lparen);
            
            Token rparen = parser_peek(parser);
            if (rparen.type == TOKEN_OPERATOR && strcmp(rparen.value, ")") == 0) {
                Token t = parser_consume(parser);
                free_token(t);
                
                while (parser_peek(parser).type == TOKEN_NEWLINE) {
                    Token nl = parser_consume(parser);
                    free_token(nl);
                }
                
                // ast_free(cmd); // No-op
                
                ASTNode *body = parse_simple_command(parser);
                if (!body) {
                    // name is on stack, no free needed
                    return NULL;
                }
                
                return ast_new_function(name, body);
            } else {
                // name on stack, cmd on stack
                return NULL;
            }
        }
        
        char *alias_val = alias_get(name);
        if (alias_val) {
            Lexer alias_lexer;
            lexer_init(&alias_lexer, alias_val);
            Token at;
            while ((at = lexer_next_token(&alias_lexer)).type != TOKEN_EOF) {
                if (at.type == TOKEN_WORD) {
                    ast_command_add_arg(cmd, at.value);
                    seen_command_name = 1; 
                }
                free_token(at);
            }
            free(alias_val);
        } else {
            ast_command_add_arg(cmd, name);
            seen_command_name = 1;
        }
        // name on stack, no free
    }
    
    parse_loop:
    while (1) {
        token = parser_peek(parser);
        
        if (token.type == TOKEN_WORD) {
            char *eq = strchr(token.value, '=');
            if (!seen_command_name && eq != NULL && eq != token.value) {
                *eq = '\0';
                char *name = token.value;
                char *value = eq + 1;
                
                ast_command_add_assignment(cmd, name, value);
                
                *eq = '='; 
                
                token = parser_consume(parser);
                free_token(token);
            } else {
                seen_command_name = 1;
                token = parser_consume(parser);
                ast_command_add_arg(cmd, token.value);
                free_token(token);
            }
        } else if (token.type == TOKEN_IO_NUMBER || token.type == TOKEN_OPERATOR) {
            if (!parse_redirection(parser, cmd)) {
                break;
            }
        } else {
            break;
        }
    }
    
    if (cmd->data.command.arg_count == 0 && 
        cmd->data.command.redirection_count == 0 &&
        cmd->data.command.assignment_count == 0) {
        // ast_free(cmd); // No-op
        return NULL;
    }
    
    return cmd;
}

static int parse_redirection(Parser *parser, ASTNode *cmd) {
    Token token = parser_peek(parser);
    int io_number = -1;
    
    if (token.type == TOKEN_IO_NUMBER) {
        io_number = atoi(token.value);
        parser_consume(parser);
        free_token(token);
        token = parser_peek(parser);
    }
    
    if (token.type != TOKEN_OPERATOR) return 0;
    
    // Check operator type
    RedirectionType type;
    if (strcmp(token.value, "<") == 0) type = REDIR_IN;
    else if (strcmp(token.value, ">") == 0) type = REDIR_OUT;
    else if (strcmp(token.value, ">>") == 0) type = REDIR_APPEND;
    else if (strcmp(token.value, ">|") == 0) type = REDIR_OUT_CLOBBER;
    else if (strcmp(token.value, "<&") == 0) type = REDIR_IN_DUP;
    else if (strcmp(token.value, ">&") == 0) type = REDIR_OUT_DUP;
    else if (strcmp(token.value, "<>") == 0) type = REDIR_RDWR;
    else if (strcmp(token.value, "<<") == 0) type = REDIR_HEREDOC;
    else if (strcmp(token.value, "<<-") == 0) type = REDIR_HEREDOC_DASH;
    else return 0;
    
    // Set default io_number if not specified
    if (io_number == -1) {
        if (type == REDIR_IN || type == REDIR_IN_DUP || type == REDIR_HEREDOC || 
            type == REDIR_HEREDOC_DASH || type == REDIR_RDWR) {
            io_number = STDIN_FILENO;  // 0
        } else {
            io_number = STDOUT_FILENO; // 1
        }
    }
    
    parser_consume(parser);
    free_token(token);
    
    Token filename = parser_consume(parser);
    if (filename.type != TOKEN_WORD) {
        // Error
        free_token(filename);
        return 0;
    }
    
    char *here_doc_content = NULL;
    if (type == REDIR_HEREDOC || type == REDIR_HEREDOC_DASH) {
        Token next = parser_peek(parser);
        if (next.type == TOKEN_NEWLINE) {
            parser_consume(parser); // Consume newline
            free_token(next);
            here_doc_content = lexer_read_until_delimiter(parser->lexer, filename.value, (type == REDIR_HEREDOC_DASH));
        } else {
             here_doc_content = lexer_read_until_delimiter(parser->lexer, filename.value, (type == REDIR_HEREDOC_DASH));
        }
    }
    
    ast_command_add_redirection(cmd, type, io_number, filename.value, here_doc_content);
    if (here_doc_content) free(here_doc_content);
    free_token(filename);
    return 1;
}
