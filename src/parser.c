/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "error.h"
#include "alias.h"
#include "variables.h"
#include "memalloc.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Forward declarations
typedef struct {
    Lexer *lexer;
    Token current_token;
    int has_token;
} Parser;

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
    Parser parser = {lexer, {0, NULL}, 0};
    
    // Parse a list (top level)
    ASTNode *node = parse_list(&parser);
    
    // Clean up any remaining token in parser
    if (parser.has_token) {
        free_token(parser.current_token);
    }
    
    // If parse_list returns NULL (empty input), return empty command
    if (!node) {
        node = ast_new_command(); // Empty command is a no-op
    }
    
    return node;
}
static ASTNode *parse_compound_list(Parser *parser, const char *terminator);

static ASTNode *parse_if_statement(Parser *parser) {
    // Expect 'if'
    Token token = parser_consume(parser);
    free_token(token);
    
    ASTNode *condition = parse_compound_list(parser, "then");
    if (!condition) return NULL;
    
    // Expect 'then' (already consumed by parse_compound_list if it returned successfully?)
    // No, parse_compound_list should peek and stop at terminator.
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "then") != 0) {
        // Error: expected 'then'
        ast_free(condition);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    ASTNode *then_branch = parse_compound_list(parser, "else"); // Or 'fi'
    // Actually, terminator could be 'else' or 'fi'.
    // parse_compound_list needs to handle multiple possible terminators or we check after.
    // Let's make parse_compound_list stop at any reserved word that isn't start of new command?
    // Or pass a list of terminators.
    // Simplified: parse_compound_list stops at 'else' or 'fi'.
    
    token = parser_peek(parser);
    ASTNode *else_branch = NULL;
    
    if (token.type == TOKEN_KEYWORD && strcmp(token.value, "else") == 0) {
        token = parser_consume(parser);
        free_token(token);
        else_branch = parse_compound_list(parser, "fi");
        token = parser_peek(parser);
    }
    
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "fi") != 0) {
        // Error: expected 'fi'
        ast_free(condition);
        ast_free(then_branch);
        if (else_branch) ast_free(else_branch);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    return ast_new_if(condition, then_branch, else_branch);
}

static ASTNode *parse_while_loop(Parser *parser);
static int parse_redirection(Parser *parser, ASTNode *cmd);
static ASTNode *parse_while_loop(Parser *parser) {
    // Expect 'while'
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
    
    return ast_new_while(condition, body);
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
    free_token(token);
    
    // Expect variable name
    token = parser_peek(parser);
    if (token.type != TOKEN_WORD) {
        return NULL;
    }
    char *var_name = xstrdup(token.value);
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
            word_list = xrealloc(word_list, sizeof(char*) * (word_count + 1));
            word_list[word_count++] = xstrdup(token.value);
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
        free(var_name);
        if (word_list) {
            for (size_t i = 0; i < word_count; i++) free(word_list[i]);
            free(word_list);
        }
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    // Parse body
    ASTNode *body = parse_compound_list(parser, "done");
    
    // Expect 'done'
    token = parser_peek(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "done") != 0) {
        free(var_name);
        if (word_list) {
            for (size_t i = 0; i < word_count; i++) free(word_list[i]);
            free(word_list);
        }
        if (body) ast_free(body);
        return NULL;
    }
    token = parser_consume(parser);
    free_token(token);
    
    return ast_new_for(var_name, word_list, word_count, body);
}

static ASTNode *parse_case_statement(Parser *parser) {
    // Expect 'case'
    Token token = parser_consume(parser);
    free_token(token);

    // Expect word
    token = parser_peek(parser);
    if (token.type != TOKEN_WORD) return NULL;
    char *word = xstrdup(token.value);
    token = parser_consume(parser);
    free_token(token);

    // Expect 'in'
    // Skip newlines before 'in' is NOT allowed by POSIX grammar for case_clause: Case WORD In ...
    // But let's check if we need to skip newlines.
    // Grammar: "case" WORD linebreak "in" linebreak case_list "esac"
    // Wait, linebreak is optional before 'in'? No.
    // "case" WORD linebreak "in" ...
    // Actually, looking at POSIX:
    // case_clause : Case WORD linebreak In linebreak case_list Esac
    //             | Case WORD linebreak In linebreak case_list_ns Esac
    //             | Case WORD linebreak In linebreak Esac
    // linebreak is optional newline(s).
    
    // So we should skip newlines after WORD.
    while (parser_peek(parser).type == TOKEN_NEWLINE) {
        Token nl = parser_consume(parser);
        free_token(nl);
    }
    
    token = parser_consume(parser);
    if (token.type != TOKEN_KEYWORD || strcmp(token.value, "in") != 0) {
        free(word);
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
            // Error or empty?
            // "esac" check above handles empty case.
            // If we are here, we expect a pattern.
            // Clean up and return NULL?
            break; 
        }

        while (1) {
            if (token.type == TOKEN_WORD) {
                patterns = xrealloc(patterns, sizeof(char*) * (pat_count + 2));
                patterns[pat_count++] = xstrdup(token.value);
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
            // Error
            // cleanup
            // For now just break
            break;
        }
        Token t = parser_consume(parser);
        free_token(t);

        // Parse commands until ';;' or 'esac'
        // We can use parse_list but we need to stop at ';;'
        // parse_list now handles ;; and esac.
        
        ASTNode *commands = NULL;
        
        // Loop to parse multiple command lists (separated by newlines)
        // Wait, parse_list handles newlines as separators and chains them.
        // So calling parse_list ONCE should parse until a terminator keyword/operator.
        // But parse_list returns NULL if it sees a terminator immediately.
        
        // Skip newlines before commands?
        while (parser_peek(parser).type == TOKEN_NEWLINE) {
             Token nl = parser_consume(parser);
             free_token(nl);
        }
        
        commands = parse_list(parser);
        
        // Add item
        items = xrealloc(items, sizeof(CaseItem) * (item_count + 1));
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
        // Error
        free_token(token);
        // cleanup
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
    // This is tricky because parse_list consumes everything until EOF or newline?
    // We need parse_list to stop at keywords.
    // But parse_list calls parse_pipeline.
    // We need a way to peek for keywords at the list level.
    
    // Let's implement a simplified version that calls parse_list but checks for terminators.
    // Actually, parse_list currently consumes newlines/semicolons.
    // We need to modify parse_list or create a new function that respects keywords.
    
    // For now, let's assume parse_list parses ONE list (pipeline sequence).
    // A compound list is a sequence of lists.
    
    ASTNode *head = NULL;
    
    while (1) {
        Token token = parser_peek(parser);
        if (token.type == TOKEN_KEYWORD) {
            if (terminator && strcmp(token.value, terminator) == 0) break;
            if (strcmp(token.value, "else") == 0 || strcmp(token.value, "fi") == 0 || 
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
            // Link them? ASTNode doesn't have 'next'.
            // We use NODE_LIST for sequencing.
            // So we need to wrap them.
            // head = ast_new_list(head, node, 0); // 0 for sequential
            // But parse_list already returns a LIST node if there are semicolons.
            // If we have multiple lines, they are effectively separated by ; (newline is like ;).
            // So we can treat them as a large list.
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
    char *name = xstrdup(token.value);
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
            free(name);
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
        free(name);
        return NULL;
    }
    
    return ast_new_function(name, body);
}

static ASTNode *parse_simple_command(Parser *parser) {
    Token token = parser_peek(parser);
    
    // A command can start with a WORD, or a redirection (which might start with IO_NUMBER or OPERATOR)
    // If it starts with IO_NUMBER, it must be a redirection.
    // If it starts with OPERATOR, it must be a redirection operator.
    // If it starts with WORD, it could be command name or assignment (not handled yet).
    
    int is_cmd = 0;
    if (token.type == TOKEN_WORD) is_cmd = 1;
    else if (token.type == TOKEN_OPERATOR && strcmp(token.value, "(") == 0) {
        // Subshell grouping: ( command_list )
        token = parser_consume(parser); // consume '('
        free_token(token);
        
        ASTNode *body = parse_list(parser);
        
        token = parser_peek(parser);
        if (token.type != TOKEN_OPERATOR || strcmp(token.value, ")") != 0) {
            if (body) ast_free(body);
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
        // Other keywords like 'then', 'else' should not start a command here (unless syntax error)
        // But if they appear, we shouldn't parse them as simple command.
        return NULL; 
    }
    else if (token.type == TOKEN_IO_NUMBER) is_cmd = 1; // "2>file" is valid start
    else if (token.type == TOKEN_OPERATOR) {
        // Check if it is a redirection operator
        if (strcmp(token.value, "<") == 0 || strcmp(token.value, ">") == 0 ||
            strcmp(token.value, ">>") == 0 || strcmp(token.value, "<<") == 0 ||
            strcmp(token.value, "<&") == 0 || strcmp(token.value, ">&") == 0 ||
            strcmp(token.value, "<>") == 0 || strcmp(token.value, ">|") == 0) {
            is_cmd = 1;
        }
    }
    
    if (!is_cmd) return NULL;
    
    ASTNode *cmd = ast_new_command();
    int seen_command_name = 0;
    
    // Check for alias if it's a WORD
    // Check for function definition or alias if it's a WORD
    // Check for alias if it's a WORD
    // Check for function definition or alias if it's a WORD
    if (token.type == TOKEN_WORD) {
        // Check for "function" keyword extension
        if (strcmp(token.value, "function") == 0) {
            Token next = parser_peek(parser);
            if (next.type == TOKEN_WORD) {
                return parse_function_definition(parser);
            }
        }

        // Check if it is an assignment FIRST
        char *eq = strchr(token.value, '=');
        if (eq && eq != token.value) {
            // Potential assignment. Check if name is valid.
            size_t name_len = eq - token.value;
            char *name_part = strndup(token.value, name_len);
            int is_valid = var_is_valid_name(name_part);
            free(name_part);
            
            if (is_valid) {
                // It IS an assignment.
                // Skip the "consume as command name" block.
                goto parse_loop;
            }
        }

        Token name_token = parser_consume(parser);
        char *name = xstrdup(name_token.value);
        free_token(name_token);
        
        Token next = parser_peek(parser);
        if (next.type == TOKEN_OPERATOR && strcmp(next.value, "(") == 0) {
            // Function definition: name ( ) compound_command
            Token lparen = parser_consume(parser);
            free_token(lparen);
            
            Token rparen = parser_peek(parser);
            if (rparen.type == TOKEN_OPERATOR && strcmp(rparen.value, ")") == 0) {
                Token t = parser_consume(parser);
                free_token(t);
                
                // Skip newlines (optional linebreak)
                while (parser_peek(parser).type == TOKEN_NEWLINE) {
                    Token nl = parser_consume(parser);
                    free_token(nl);
                }
                
                // Parse function body
                // We try to parse a compound command.
                // Since we don't have a dedicated parse_compound_command, we use parse_simple_command
                // which dispatches to compound commands.
                // However, we must ensure it IS a compound command or at least a valid body.
                // POSIX requires compound command.
                // If parse_simple_command returns a simple command, strictly it's invalid, but we can allow it.
                
                // We need to free the empty command we created at the start of this function
                ast_free(cmd);
                
                ASTNode *body = parse_simple_command(parser);
                if (!body) {
                    free(name);
                    return NULL;
                }
                
                return ast_new_function(name, body);
            } else {
                // Error: expected )
                // We consumed '(', so we can't backtrack easily.
                // Treat as syntax error.
                free(name);
                ast_free(cmd);
                return NULL;
            }
        }
        
        // Not a function. Check alias.
        char *alias_val = alias_get(name);
        if (alias_val) {
            // Alias found!
            // Parse alias value
            Lexer alias_lexer;
            lexer_init(&alias_lexer, alias_val);
            Token at;
            while ((at = lexer_next_token(&alias_lexer)).type != TOKEN_EOF) {
                if (at.type == TOKEN_WORD) {
                    ast_command_add_arg(cmd, at.value);
                    seen_command_name = 1; 
                }
                // TODO: Handle redirections in alias?
                free_token(at);
            }
            free(alias_val);
        } else {
            // Normal command name
            ast_command_add_arg(cmd, name);
            seen_command_name = 1;
        }
        free(name);
    }
    
    // If we are here, we might have processed an alias or function check.
    // If it was a function, we returned early.
    // If it was an alias, we added args to 'cmd' and set 'seen_command_name'.
    // If it was neither (normal word), we added it to 'cmd' and set 'seen_command_name'.
    // If it wasn't a WORD at start, we skipped the block above.

    // Continue parsing arguments and redirections

    // If no alias was found, or if alias was processed, continue with normal parsing
    // This loop handles arguments, assignments, and redirections for the command.
    // If an alias was processed, 'cmd' already has its initial args.
    // If no alias, 'cmd' is empty and 'seen_command_name' is 0.
    parse_loop:
    while (1) {
        token = parser_peek(parser);
        
        if (token.type == TOKEN_WORD) {
            // Check for assignment (NAME=VALUE)
            // Only if we haven't seen the command name yet
            char *eq = strchr(token.value, '=');
            if (!seen_command_name && eq != NULL && eq != token.value) {
                // Valid assignment? NAME must be valid identifier
                // For now, assume yes if it contains =
                // Split into name and value
                *eq = '\0';
                char *name = token.value;
                char *value = eq + 1;
                
                ast_command_add_assignment(cmd, name, value);
                
                // Restore token value for freeing (though we strdup'd)
                *eq = '='; 
                
                token = parser_consume(parser);
                free_token(token);
            } else {
                // It's a command name or argument
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
        ast_free(cmd);
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
        // Read here-doc content
        // We need to skip the newline that follows the command line?
        // The lexer_read_until_delimiter handles reading from current pos.
        // But usually here-doc starts after the current command line is finished?
        // POSIX: "The here-document shall be treated as a single word that begins after the next newline".
        // This implies we should wait until we see a newline?
        // But my parser parses the command line.
        // If I read now, I might consume tokens that belong to the command?
        // No, because here-doc content is NOT tokens.
        // But if the command is `cat <<EOF; echo hi`, the `echo hi` is on the same line.
        // The here-doc starts AFTER the newline.
        // So `echo hi` should be parsed first.
        // But `parse_redirection` is called while parsing `cat`.
        
        // This is tricky. The here-doc content is physically located after the newline.
        // But logically it belongs to the redirection.
        
        // If I read it now, I must ensure I am at the newline?
        // If I am at `<<EOF`, the next token might be `;` or `newline`.
        // If `;`, then `echo hi` follows. Then `newline`. Then here-doc content.
        
        // My simple `lexer_read_until_delimiter` reads from CURRENT position.
        // This is wrong if there are other commands on the line.
        
        // However, for `posish -c "cat <<EOF\nhello\nEOF"`, the newline is immediate.
        // If `posish -c "cat <<EOF; echo hi\nhello\nEOF"`, then `echo hi` is parsed.
        
        // To support this properly, I should queue the here-doc reading until I see a newline?
        // Or just assume for now that here-doc is the last thing or immediately follows?
        
        // Let's assume simple case: `cat <<EOF` is followed by newline.
        // If I consume `EOF` (filename), the next token should be newline.
        // I can peek.
        
        Token next = parser_peek(parser);
        if (next.type == TOKEN_NEWLINE) {
            parser_consume(parser); // Consume newline
            free_token(next);
            here_doc_content = lexer_read_until_delimiter(parser->lexer, filename.value, (type == REDIR_HEREDOC_DASH));
        } else {
            // If not newline, maybe we should warn?
            // Or maybe we just read anyway?
            // If I read anyway, I might consume `echo hi` as part of here-doc if `EOF` is not found.
            // But `echo hi` is not `EOF`.
            
            // If I implement strict POSIX, I need to defer reading.
            // But that requires major parser restructuring.
            
            // For now, I will read immediately, assuming the user puts newline after `<<EOF`.
            // And if there are other commands, they must be on next lines?
            // Wait, if I read immediately, I consume the rest of the string.
            
            // Let's try to read immediately.
             here_doc_content = lexer_read_until_delimiter(parser->lexer, filename.value, (type == REDIR_HEREDOC_DASH));
        }
    }
    
    ast_command_add_redirection(cmd, type, io_number, filename.value, here_doc_content);
    if (here_doc_content) free(here_doc_content);
    free_token(filename);
    return 1;
}
