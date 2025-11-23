/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "memalloc.h"

// Simple operators for now. POSIX defines more (&&, ||, ;;, <<, >>, <&, >&, <>, <<-, >|)
static const char *OPERATORS[] = {
    "&&", "||", ";;", "<<", ">>", "<&", ">&", "<>", ">|", // 2-char ops first
    "|", "&", ";", "<", ">", "(", ")", "\n", NULL
};

void lexer_init(Lexer *lexer, const char *input) {
    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    lexer->current_line = 1;
}

void free_token(Token token) {
    // Check if value is dynamically allocated (not pointing to OPERATORS array)
    if (token.value) {
        int is_static = 0;
        for (int i = 0; OPERATORS[i]; i++) {
            if (token.value == OPERATORS[i] || token.value == (char*)"\n") {
                is_static = 1;
                break;
            }
        }
        if (!is_static) {
            free(token.value);
        }
    }
}

static int is_operator_start(char c) {
    for (int i = 0; OPERATORS[i]; i++) {
        if (OPERATORS[i][0] == c) return 1;
    }
    return 0;
}

// Check if the current sequence matches an operator
// Returns length of operator if match, 0 otherwise
static int match_operator(const char *str) {
    // TODO: Handle multi-char operators like >>, <<, &&, ||
    // For now, just single char match from our list
    for (int i = 0; OPERATORS[i]; i++) {
        if (strncmp(str, OPERATORS[i], strlen(OPERATORS[i])) == 0) {
            return strlen(OPERATORS[i]);
        }
    }
    return 0;
}

static void lexer_advance(Lexer *lexer) {
    if (lexer->pos < lexer->len) {
        if (lexer->input[lexer->pos] == '\n') {
            lexer->current_line++;
        }
        lexer->pos++;
    }
}

Token lexer_next_token(Lexer *lexer) {
    Token token = {TOKEN_EOF, NULL, lexer->current_line};
    
    while (lexer->pos < lexer->len && isspace(lexer->input[lexer->pos]) && lexer->input[lexer->pos] != '\n') {
        lexer_advance(lexer);
    }

    if (lexer->pos >= lexer->len) {
        return token;
    }

    char c = lexer->input[lexer->pos];

    int op_len = match_operator(lexer->input + lexer->pos);
    if (op_len > 0) {        if (lexer->input[lexer->pos] == '\n') {
            token.type = TOKEN_NEWLINE;
            token.value = (char*)"\n"; // Static string, no allocation
            lexer_advance(lexer);
            return token;
        }

        token.type = TOKEN_OPERATOR;
        // Find matching operator and return pointer to it
       for (int i = 0; OPERATORS[i]; i++) {
            if (strncmp(lexer->input + lexer->pos, OPERATORS[i], op_len) == 0 && strlen(OPERATORS[i]) == (size_t)op_len) {
                token.value = (char*)OPERATORS[i]; // Static string, no allocation
                for (int k=0; k<op_len; k++) lexer_advance(lexer);
                return token;
            }
        }
        // Fallback if no match found
        token.value = xmalloc(op_len + 1);
        strncpy(token.value, lexer->input + lexer->pos, op_len);
        token.value[op_len] = '\0';
        for (int k=0; k<op_len; k++) lexer_advance(lexer);
        return token;
    }

    if (c == '#') {
        while (lexer->pos < lexer->len && lexer->input[lexer->pos] != '\n') {
            lexer_advance(lexer);
        }
        return lexer_next_token(lexer);
    }

    size_t capacity = 64;
    char *buffer = xmalloc(capacity);
    size_t buf_idx = 0;
    
    // Macro to ensure buffer has space for at least N more chars
    #define ENSURE_BUFFER_SPACE(n) do { \
        while (buf_idx + (n) >= capacity) { \
            capacity *= 2; \
            buffer = xrealloc(buffer, capacity); \
        } \
    } while(0)
    
    while (lexer->pos < lexer->len) {
        char current = lexer->input[lexer->pos];

        if (current == '\\') {
            lexer_advance(lexer);
            if (lexer->pos < lexer->len) {
                if (lexer->input[lexer->pos] == '\n') {
                    // Line continuation: skip both backslash and newline
                    lexer_advance(lexer);
                    continue;
                }
                ENSURE_BUFFER_SPACE(1);
                buffer[buf_idx++] = lexer->input[lexer->pos];
                lexer_advance(lexer);
            }
        } else if (current == '\'') {
            ENSURE_BUFFER_SPACE(1);
            buffer[buf_idx++] = current;
            lexer_advance(lexer);
            while (lexer->pos < lexer->len) {
                if (lexer->input[lexer->pos] == '\'') {
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = lexer->input[lexer->pos];
                    lexer_advance(lexer);
                    break;
                }
                ENSURE_BUFFER_SPACE(1);
                buffer[buf_idx++] = lexer->input[lexer->pos];
                lexer_advance(lexer);
            }
        } else if (current == '"') {
            ENSURE_BUFFER_SPACE(1);
            buffer[buf_idx++] = current;
            lexer_advance(lexer);
            while (lexer->pos < lexer->len) {
                if (lexer->input[lexer->pos] == '"') {
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = lexer->input[lexer->pos];
                    lexer_advance(lexer);
                    break;
                }
                if (lexer->input[lexer->pos] == '\\') {
                    lexer_advance(lexer);
                    if (lexer->pos < lexer->len) {
                        char next = lexer->input[lexer->pos];
                        if (next == '\n') {
                            // Line continuation inside double quotes
                            lexer_advance(lexer);
                            continue;
                        }
                        if (next == '$' || next == '`' || next == '"' || next == '\\') {
                            ENSURE_BUFFER_SPACE(2);
                            buffer[buf_idx++] = '\\';
                            buffer[buf_idx++] = next;
                        } else {
                            ENSURE_BUFFER_SPACE(2);
                            buffer[buf_idx++] = '\\';
                            buffer[buf_idx++] = next;
                        }
                        lexer_advance(lexer);
                    }
                } else {
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = lexer->input[lexer->pos];
                    lexer_advance(lexer);
                }
            }
        } else if (current == '$') {
             if (lexer->pos + 1 < lexer->len && lexer->input[lexer->pos + 1] == '(') {
                ENSURE_BUFFER_SPACE(2);
                buffer[buf_idx++] = current;
                lexer_advance(lexer);
                buffer[buf_idx++] = '(';
                lexer_advance(lexer);

                
                int nesting = 1;
                while (lexer->pos < lexer->len && nesting > 0) {
                    ENSURE_BUFFER_SPACE(1);
                    char next = lexer->input[lexer->pos];
                    if (next == '(') nesting++;
                    else if (next == ')') nesting--;
                    
                    buffer[buf_idx++] = next;
                    lexer_advance(lexer);
                }
            } else if (lexer->pos + 1 < lexer->len && lexer->input[lexer->pos + 1] == '{') {
                // Parameter expansion ${...}
                buffer[buf_idx++] = current;
                lexer_advance(lexer);
                ENSURE_BUFFER_SPACE(1);
                buffer[buf_idx++] = current;
                lexer_advance(lexer);
                ENSURE_BUFFER_SPACE(1);
                buffer[buf_idx++] = '{';
                lexer_advance(lexer);
                
                int nesting = 1;
                int in_single = 0;
                int in_double = 0;
                
                while (lexer->pos < lexer->len && nesting > 0) {
                    ENSURE_BUFFER_SPACE(1);
                    char next = lexer->input[lexer->pos];
                    
                    if (in_single) {
                        if (next == '\'') in_single = 0;
                    } else if (in_double) {
                        if (next == '"') in_double = 0;
                        else if (next == '\\' && lexer->pos + 1 < lexer->len) {
                            ENSURE_BUFFER_SPACE(1);
                            buffer[buf_idx++] = next;
                            lexer_advance(lexer);
                            next = lexer->input[lexer->pos];
                        }
                    } else {
                        if (next == '\'') in_single = 1;
                        else if (next == '"') in_double = 1;
                        else if (next == '{') nesting++;
                        else if (next == '}') nesting--;
                    }
                    
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = next;
                    lexer_advance(lexer);
                }
            } else {
                ENSURE_BUFFER_SPACE(1);
                buffer[buf_idx++] = current;
                lexer_advance(lexer);
            }
        } else if (current == '`') {
            ENSURE_BUFFER_SPACE(1);
            buffer[buf_idx++] = current;
            lexer_advance(lexer);
            while (lexer->pos < lexer->len) {
                if (lexer->input[lexer->pos] == '`') {
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = lexer->input[lexer->pos];
                    lexer_advance(lexer);
                    break;
                }
                if (lexer->input[lexer->pos] == '\\') {
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = lexer->input[lexer->pos];
                    lexer_advance(lexer);
                    if (lexer->pos < lexer->len) {
                        ENSURE_BUFFER_SPACE(1);
                        buffer[buf_idx++] = lexer->input[lexer->pos];
                        lexer_advance(lexer);
                    }
                } else {
                    ENSURE_BUFFER_SPACE(1);
                    buffer[buf_idx++] = lexer->input[lexer->pos];
                    lexer_advance(lexer);
                }
            }
        } else if (isspace(current)) {
            break;
        } else if (is_operator_start(current)) {
             break;
        } else {
            ENSURE_BUFFER_SPACE(1);
            buffer[buf_idx++] = current;
            lexer_advance(lexer);
        }
    }

    ENSURE_BUFFER_SPACE(1);
    buffer[buf_idx] = '\0';
    token.type = TOKEN_WORD;
    token.value = buffer;
    
    
    const char *keywords[] = {"if", "then", "else", "fi", "while", "until", "for", "in", "do", "done", "case", "esac", "{", "}", NULL};
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(buffer, keywords[i]) == 0) {
            token.type = TOKEN_KEYWORD;
            break;
        }
    }

    if (lexer->pos < lexer->len) {
        char next = lexer->input[lexer->pos];
        if (next == '<' || next == '>') {
            int all_digits = 1;
            for (size_t i = 0; i < buf_idx; i++) {
                if (!isdigit(buffer[i])) {
                    all_digits = 0;
                    break;
                }
            }
            if (all_digits && buf_idx > 0) {
                token.type = TOKEN_IO_NUMBER;
            }
        }
    }

    return token;
}

char *lexer_read_until_delimiter(Lexer *lexer, const char *delimiter, int strip_tabs) {
    size_t capacity = 1024;
    size_t length = 0;
    char *content = xmalloc(capacity);
    if (!content) return NULL;
    content[0] = '\0';
    
    while (lexer->pos < lexer->len) {
        // Find end of line
        size_t start = lexer->pos;
        size_t end = start;
        while (end < lexer->len && lexer->input[end] != '\n') {
            end++;
        }
        
        // Extract line
        size_t line_len = end - start;
        char *line = xmalloc(line_len + 1);
        strncpy(line, lexer->input + start, line_len);
        line[line_len] = '\0';
        
        // Advance lexer past newline
        lexer->pos = end;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '\n') {
            lexer->pos++;
        }
        
        // Check delimiter
        char *check_line = line;
        if (strip_tabs) {
            while (*check_line == '\t') check_line++;
        }
        
        if (strcmp(check_line, delimiter) == 0) {
            free(line);
            break;
        }
        
        // Append to content
        char *append_line = line;
        if (strip_tabs) {
            while (*append_line == '\t') append_line++;
        }
        
        size_t append_len = strlen(append_line);
        if (length + append_len + 2 > capacity) {
            capacity *= 2;
            content = xrealloc(content, capacity);
        }
        
        strcat(content, append_line);
        strcat(content, "\n");
        length += append_len + 1;
        
        free(line);
    }
    
    return content;
}
// Check if input is incomplete (unclosed quotes, trailing backslash)
int lexer_check_incomplete(const char *input) {
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    size_t len = strlen(input);

    for (size_t i = 0; i < len; i++) {
        char c = input[i];

        if (escaped) {
            // If we escaped a newline, and it's the end of the string, it's a continuation!
            // But wait, if we are here, we consumed the char after backslash.
            // If that char was '\n', and it was the last char, then we are incomplete?
            // No, read_line returns "line\n".
            // So if input is "line\\\n", we see '\' then '\n'.
            // At '\', escaped=1.
            // At '\n', we enter this block. escaped=0.
            // We need to detect that this was a backslash-newline sequence at the end.
            
            if (c == '\n' && i == len - 1) {
                return 3; // Trailing backslash continuation
            }
            escaped = 0;
            continue;
        }

        if (c == '\\') {
            // Backslash escapes next char if:
            // 1. Outside quotes
            // 2. Inside double quotes (only specific chars, but for completeness check we can be loose)
            // Actually, inside single quotes backslash is literal.
            if (!in_single) {
                escaped = 1;
            }
        } else if (c == '\'') {
            if (!in_double) {
                in_single = !in_single;
            }
        } else if (c == '"') {
            if (!in_single) {
                in_double = !in_double;
            }
        }
    }

    if (in_single) return 1;
    if (in_double) return 2;
    if (escaped) return 3; // Trailing backslash
    return 0;
}
