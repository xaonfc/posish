/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_WORD,
    TOKEN_KEYWORD,
    TOKEN_OPERATOR,
    TOKEN_IO_NUMBER,
    TOKEN_NEWLINE,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *value; // malloc'd string, caller must free
    int lineno; // Line number where token starts
} Token;

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
    int current_line; // Current line number
} Lexer;

void lexer_init(Lexer *lexer, const char *input);
Token lexer_next_token(Lexer *lexer);
char *lexer_read_until_delimiter(Lexer *lexer, const char *delimiter, int strip_tabs);
void free_token(Token token);

// Check if input is incomplete (unclosed quotes, trailing backslash)
// Returns 0 if complete, >0 if incomplete
int lexer_check_incomplete(const char *input);

#endif
