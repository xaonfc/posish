/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "memalloc.h"

ASTNode *ast_new_command(void) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_COMMAND;
    node->data.command.args = NULL;
    node->data.command.arg_count = 0;
    node->data.command.redirections = NULL;
    node->data.command.redirection_count = 0;
    node->data.command.assignments = NULL;
    node->data.command.assignment_count = 0;
    return node;
}

void ast_command_add_arg(ASTNode *node, const char *arg) {
    if (node->type != NODE_COMMAND) return;
    
    node->data.command.arg_count++;
    node->data.command.args = xrealloc(node->data.command.args, 
                                      sizeof(char*) * (node->data.command.arg_count + 1));
    
    node->data.command.args[node->data.command.arg_count - 1] = xstrdup(arg);
    node->data.command.args[node->data.command.arg_count] = NULL;
}

void ast_command_add_assignment(ASTNode *node, const char *name, const char *value) {
    if (node->type != NODE_COMMAND) return;
    
    node->data.command.assignment_count++;
    node->data.command.assignments = xrealloc(node->data.command.assignments, 
                                             sizeof(node->data.command.assignments[0]) * node->data.command.assignment_count);
    
    node->data.command.assignments[node->data.command.assignment_count - 1].name = xstrdup(name);
    node->data.command.assignments[node->data.command.assignment_count - 1].value = xstrdup(value);
}

void ast_command_add_redirection(ASTNode *node, RedirectionType type, int io_number, const char *filename, const char *here_doc_content) {
    if (node->type != NODE_COMMAND) return;
    
    node->data.command.redirections = xrealloc(node->data.command.redirections, 
        sizeof(Redirection) * (node->data.command.redirection_count + 1));
        
    Redirection *r = &node->data.command.redirections[node->data.command.redirection_count++];
    r->type = type;
    r->io_number = io_number;
    r->filename = filename ? xstrdup(filename) : NULL;
    r->here_doc_content = here_doc_content ? xstrdup(here_doc_content) : NULL;
}

ASTNode *ast_new_pipeline(ASTNode *left, ASTNode *right) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_PIPELINE;
    node->data.pipeline.left = left;
    node->data.pipeline.right = right;
    return node;
}

ASTNode *ast_new_list(ASTNode *left, ASTNode *right, int async) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_LIST;
    node->data.list.left = left;
    node->data.list.right = right;
    node->data.list.async = async;
    return node;
}

ASTNode *ast_new_if(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_IF;
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;
    return node;
}

ASTNode *ast_new_while(ASTNode *condition, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_WHILE;
    node->data.while_loop.condition = condition;
    node->data.while_loop.body = body;
    return node;
}

ASTNode *ast_new_until(ASTNode *condition, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_UNTIL;
    node->data.until_loop.condition = condition;
    node->data.until_loop.body = body;
    return node;
}

ASTNode *ast_new_for(const char *var_name, char **word_list, size_t word_count, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_FOR;
    node->data.for_loop.var_name = xstrdup(var_name);
    node->data.for_loop.word_list = word_list;  // Takes ownership
    node->data.for_loop.word_count = word_count;
    node->data.for_loop.body = body;
    return node;
}

ASTNode *ast_new_subshell(ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_SUBSHELL;
    node->data.subshell.body = body;
    return node;
}

ASTNode *ast_new_group(ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_GROUP;
    node->data.group.body = body;
    return node;
}

ASTNode *ast_new_function(const char *name, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_FUNCTION;
    node->data.function.name = xstrdup(name);
    node->data.function.body = body;
    return node;
}

ASTNode *ast_new_case(const char *word, CaseItem *items, size_t item_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = NODE_CASE;
    node->data.case_stmt.word = xstrdup(word);
    node->data.case_stmt.items = items;  // Takes ownership
    node->data.case_stmt.item_count = item_count;
    return node;
}

ASTNode *ast_new_binary(NodeType type, ASTNode *left, ASTNode *right) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    // We can reuse pipeline struct as it has left and right
    node->data.pipeline.left = left;
    node->data.pipeline.right = right;
    return node;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    
    if (node->type == NODE_COMMAND) {
        if (node->data.command.args) {
            for (size_t i = 0; i < node->data.command.arg_count; i++) {
                free(node->data.command.args[i]);
            }
            free(node->data.command.args);
        }
        if (node->data.command.redirections) {
            for (size_t i = 0; i < node->data.command.redirection_count; i++) {
                free(node->data.command.redirections[i].filename);
                free(node->data.command.redirections[i].here_doc_content);
            }
            free(node->data.command.redirections);
        }
        if (node->data.command.assignments) {
            for (size_t i = 0; i < node->data.command.assignment_count; i++) {
                free(node->data.command.assignments[i].name);
                free(node->data.command.assignments[i].value);
            }
            free(node->data.command.assignments);
        }
        
    } else if (node->type == NODE_PIPELINE) {
        ast_free(node->data.pipeline.left);
        ast_free(node->data.pipeline.right);

    } else if (node->type == NODE_LIST) {
        ast_free(node->data.list.left);
        ast_free(node->data.list.right);

    } else if (node->type == NODE_IF) {
        ast_free(node->data.if_stmt.condition);
        ast_free(node->data.if_stmt.then_branch);
        ast_free(node->data.if_stmt.else_branch);

    } else if (node->type == NODE_WHILE) {
        ast_free(node->data.while_loop.condition);
        if (node->data.while_loop.body) ast_free(node->data.while_loop.body);

    } else if (node->type == NODE_UNTIL) {
        ast_free(node->data.until_loop.condition);
        if (node->data.until_loop.body) ast_free(node->data.until_loop.body);

    } else if (node->type == NODE_FOR) {
        free(node->data.for_loop.var_name);
        if (node->data.for_loop.word_list) {
            for (size_t i = 0; i < node->data.for_loop.word_count; i++) {
                free(node->data.for_loop.word_list[i]);
            }
            free(node->data.for_loop.word_list);
        }
        if (node->data.for_loop.body) ast_free(node->data.for_loop.body);

    } else if (node->type == NODE_SUBSHELL) {
        if (node->data.subshell.body) ast_free(node->data.subshell.body);

    } else if (node->type == NODE_GROUP) {
        if (node->data.group.body) ast_free(node->data.group.body);

    } else if (node->type == NODE_FUNCTION) {
        free(node->data.function.name);
        // We do NOT free the body here if it's stored in the function table?
        // Actually, when we define a function, we usually keep the AST around.
        // But if we free the definition node (e.g. after execution of the definition),
        // we might want to keep the body.
        // However, typically 'ast_free' frees the tree.
        // The executor will need to copy/clone or take ownership.
        // For now, let's assume ast_free frees everything.
        if (node->data.function.body) ast_free(node->data.function.body);

    } else if (node->type == NODE_CASE) {
        free(node->data.case_stmt.word);
        if (node->data.case_stmt.items) {
            for (size_t i = 0; i < node->data.case_stmt.item_count; i++) {
                if (node->data.case_stmt.items[i].patterns) {
                    for (int j = 0; node->data.case_stmt.items[i].patterns[j]; j++) {
                        free(node->data.case_stmt.items[i].patterns[j]);
                    }
                    free(node->data.case_stmt.items[i].patterns);
                }
                if (node->data.case_stmt.items[i].commands) {
                    ast_free(node->data.case_stmt.items[i].commands);
                }
            }
            free(node->data.case_stmt.items);
        }

    } else if (node->type == NODE_AND || node->type == NODE_OR) {
        ast_free(node->data.pipeline.left);
        ast_free(node->data.pipeline.right);
    }
    
    free(node);
}

ASTNode *ast_copy(ASTNode *node) {
    if (!node) return NULL;
    
    ASTNode *new_node = xmalloc(sizeof(ASTNode));
    new_node->type = node->type;
    
    if (node->type == NODE_COMMAND) {
        new_node->data.command.args = NULL;
        new_node->data.command.arg_count = node->data.command.arg_count;
        if (node->data.command.args) {
            new_node->data.command.args = xmalloc(sizeof(char*) * (node->data.command.arg_count + 1));
            for (size_t i = 0; i < node->data.command.arg_count; i++) {
                new_node->data.command.args[i] = xstrdup(node->data.command.args[i]);
            }
            new_node->data.command.args[node->data.command.arg_count] = NULL;
        }
        
        new_node->data.command.redirections = NULL;
        new_node->data.command.redirection_count = node->data.command.redirection_count;
        if (node->data.command.redirections) {
            new_node->data.command.redirections = xmalloc(sizeof(Redirection) * node->data.command.redirection_count);
            for (size_t i = 0; i < node->data.command.redirection_count; i++) {
                new_node->data.command.redirections[i].type = node->data.command.redirections[i].type;
                new_node->data.command.redirections[i].io_number = node->data.command.redirections[i].io_number;
                new_node->data.command.redirections[i].filename = node->data.command.redirections[i].filename ? xstrdup(node->data.command.redirections[i].filename) : NULL;
                new_node->data.command.redirections[i].here_doc_content = node->data.command.redirections[i].here_doc_content ? xstrdup(node->data.command.redirections[i].here_doc_content) : NULL;
            }
        }
        
        new_node->data.command.assignments = NULL;
        new_node->data.command.assignment_count = node->data.command.assignment_count;
        if (node->data.command.assignments) {
            new_node->data.command.assignments = xmalloc(sizeof(node->data.command.assignments[0]) * node->data.command.assignment_count);
            for (size_t i = 0; i < node->data.command.assignment_count; i++) {
                new_node->data.command.assignments[i].name = xstrdup(node->data.command.assignments[i].name);
                new_node->data.command.assignments[i].value = xstrdup(node->data.command.assignments[i].value);
            }
        }
        
    } else if (node->type == NODE_PIPELINE) {
        new_node->data.pipeline.left = ast_copy(node->data.pipeline.left);
        new_node->data.pipeline.right = ast_copy(node->data.pipeline.right);

    } else if (node->type == NODE_LIST) {
        new_node->data.list.left = ast_copy(node->data.list.left);
        new_node->data.list.right = ast_copy(node->data.list.right);
        new_node->data.list.async = node->data.list.async;

    } else if (node->type == NODE_IF) {
        new_node->data.if_stmt.condition = ast_copy(node->data.if_stmt.condition);
        new_node->data.if_stmt.then_branch = ast_copy(node->data.if_stmt.then_branch);
        new_node->data.if_stmt.else_branch = ast_copy(node->data.if_stmt.else_branch);

    } else if (node->type == NODE_WHILE) {
        new_node->data.while_loop.condition = ast_copy(node->data.while_loop.condition);
        new_node->data.while_loop.body = ast_copy(node->data.while_loop.body);

    } else if (node->type == NODE_UNTIL) {
        new_node->data.until_loop.condition = ast_copy(node->data.until_loop.condition);
        new_node->data.until_loop.body = ast_copy(node->data.until_loop.body);

    } else if (node->type == NODE_FOR) {
        new_node->data.for_loop.var_name = xstrdup(node->data.for_loop.var_name);
        new_node->data.for_loop.word_count = node->data.for_loop.word_count;
        new_node->data.for_loop.word_list = NULL;
        if (node->data.for_loop.word_list) {
            new_node->data.for_loop.word_list = xmalloc(sizeof(char*) * (node->data.for_loop.word_count + 1));
            for (size_t i = 0; i < node->data.for_loop.word_count; i++) {
                new_node->data.for_loop.word_list[i] = xstrdup(node->data.for_loop.word_list[i]);
            }
            new_node->data.for_loop.word_list[node->data.for_loop.word_count] = NULL;
        }
        new_node->data.for_loop.body = ast_copy(node->data.for_loop.body);

    } else if (node->type == NODE_SUBSHELL) {
        new_node->data.subshell.body = ast_copy(node->data.subshell.body);

    } else if (node->type == NODE_GROUP) {
        new_node->data.group.body = ast_copy(node->data.group.body);

    } else if (node->type == NODE_FUNCTION) {
        new_node->data.function.name = xstrdup(node->data.function.name);
        new_node->data.function.body = ast_copy(node->data.function.body);

    } else if (node->type == NODE_CASE) {
        new_node->data.case_stmt.word = xstrdup(node->data.case_stmt.word);
        new_node->data.case_stmt.item_count = node->data.case_stmt.item_count;
        new_node->data.case_stmt.items = NULL;
        if (node->data.case_stmt.items) {
            new_node->data.case_stmt.items = xmalloc(sizeof(CaseItem) * node->data.case_stmt.item_count);
            for (size_t i = 0; i < node->data.case_stmt.item_count; i++) {
                new_node->data.case_stmt.items[i].patterns = NULL;
                if (node->data.case_stmt.items[i].patterns) {
                    size_t pat_count = 0;
                    while (node->data.case_stmt.items[i].patterns[pat_count]) pat_count++;
                    new_node->data.case_stmt.items[i].patterns = xmalloc(sizeof(char*) * (pat_count + 1));
                    for (size_t j = 0; j < pat_count; j++) {
                        new_node->data.case_stmt.items[i].patterns[j] = xstrdup(node->data.case_stmt.items[i].patterns[j]);
                    }
                    new_node->data.case_stmt.items[i].patterns[pat_count] = NULL;
                }
                new_node->data.case_stmt.items[i].commands = ast_copy(node->data.case_stmt.items[i].commands);
            }
        }

    } else if (node->type == NODE_AND || node->type == NODE_OR) {
        new_node->data.pipeline.left = ast_copy(node->data.pipeline.left);
        new_node->data.pipeline.right = ast_copy(node->data.pipeline.right);
    }
    
    return new_node;
}
