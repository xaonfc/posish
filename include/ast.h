/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef AST_H
#define AST_H

#include <stdlib.h>

typedef enum {
    NODE_COMMAND,
    NODE_PIPELINE,
    NODE_LIST,
    NODE_IF,
    NODE_WHILE,
    NODE_UNTIL,
    NODE_FOR,
    NODE_SUBSHELL,
    NODE_CASE,
    NODE_GROUP,
    NODE_FUNCTION,
    NODE_AND,
    NODE_OR
} NodeType;

typedef struct CaseItem {
    char **patterns;      // NULL-terminated array of patterns
    struct ASTNode *commands;
} CaseItem;

typedef enum {
    REDIR_IN,         // <
    REDIR_OUT,        // >
    REDIR_OUT_CLOBBER,// >|
    REDIR_APPEND,     // >>
    REDIR_IN_DUP,     // <&
    REDIR_OUT_DUP,    // >&
    REDIR_RDWR,       // <>
    REDIR_HEREDOC,    // <<
    REDIR_HEREDOC_DASH// <<-
} RedirectionType;

typedef struct {
    RedirectionType type;
    int io_number;    // The FD being redirected (e.g., 2 in "2>file")
    char *filename;   // The target file or delimiter
    char *here_doc_content; // Content for << and <<-
} Redirection;

typedef struct ASTNode {
    NodeType type;
    int lineno; // Line number from source
    union {
        struct {
            char **args; // NULL terminated array of strings
            size_t arg_count;
            Redirection *redirections;
            size_t redirection_count;
            struct {
                char *name;
                char *value;
            } *assignments;
            size_t assignment_count;
        } command;
        struct {
            struct ASTNode *left;
            struct ASTNode *right;
        } pipeline;
        struct {
            struct ASTNode *left;
            struct ASTNode *right;
            int async; // 1 if background (&), 0 if sequential (;)
        } list;
        struct {
            struct ASTNode *condition;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch;
        } if_stmt;
        struct {
            struct ASTNode *condition;
            struct ASTNode *body;
        } while_loop;
        struct {
            struct ASTNode *condition;
            struct ASTNode *body;
        } until_loop;
        struct {
            char *var_name;
            char **word_list;  // NULL for "for name; do" variant
            size_t word_count;
            struct ASTNode *body;
        } for_loop;
        struct {
            struct ASTNode *body;
        } subshell;
        struct {
            struct ASTNode *body;
        } group;
        struct {
            char *name;
            struct ASTNode *body;
        } function;
        struct {
            char *word;
            CaseItem *items;
            size_t item_count;
        } case_stmt;
    } data;
} ASTNode;

ASTNode *ast_new_command(void);
void ast_command_add_arg(ASTNode *node, const char *arg);
void ast_command_add_redirection(ASTNode *node, RedirectionType type, int io_number, const char *filename, const char *here_doc_content);
void ast_command_add_assignment(ASTNode *node, const char *name, const char *value);
ASTNode *ast_new_pipeline(ASTNode *left, ASTNode *right);
ASTNode *ast_new_list(ASTNode *left, ASTNode *right, int async);
ASTNode *ast_new_if(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch);
ASTNode *ast_new_while(ASTNode *condition, ASTNode *body);
ASTNode *ast_new_until(ASTNode *condition, ASTNode *body);
ASTNode *ast_new_for(const char *var_name, char **word_list, size_t word_count, ASTNode *body);
ASTNode *ast_new_subshell(ASTNode *body);
ASTNode *ast_new_group(ASTNode *body);
ASTNode *ast_new_function(const char *name, ASTNode *body);
ASTNode *ast_new_case(const char *word, CaseItem *items, size_t item_count);
ASTNode *ast_new_binary(NodeType type, ASTNode *left, ASTNode *right);
void ast_free(ASTNode *node);
ASTNode *ast_copy(ASTNode *node);
void ast_free_heap(ASTNode *node);
ASTNode *ast_clone_to_heap(ASTNode *node);

#endif
