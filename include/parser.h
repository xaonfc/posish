/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

ASTNode *parser_parse(Lexer *lexer);
int parser_try_fast_path(const char *cmd);

#endif
