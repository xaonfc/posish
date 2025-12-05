/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ast.h"
#include "memalloc.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Allocator Abstraction
 * ============================================================================
 * Unified interface for stack vs heap allocation eliminates code duplication
 * in clone/copy operations.
 */

typedef struct {
    void *(*alloc)(size_t size);
    void *(*realloc_array)(void *ptr, size_t old_cnt, size_t new_cnt, size_t elem_sz);
    char *(*strdup)(const char *s);
} Allocator;

static void *heap_realloc_array(void *ptr, size_t old_cnt, size_t new_cnt, size_t elem_sz) {
    (void)old_cnt;
    return xrealloc(ptr, new_cnt * elem_sz);
}

static const Allocator STACK_ALLOC = {
    .alloc         = mem_stack_alloc,
    .realloc_array = mem_stack_realloc_array,
    .strdup        = mem_stack_strdup,
};

static const Allocator HEAP_ALLOC = {
    .alloc         = xmalloc,
    .realloc_array = heap_realloc_array,
    .strdup        = xstrdup,
};

/* ============================================================================
 * String Utilities
 * ============================================================================ */

static inline char *dup_str(const Allocator *a, const char *s) {
    return s ? a->strdup(s) : NULL;
}

static char **dup_str_array(const Allocator *a, char **src, size_t count) {
    if (!src) return NULL;

    char **dst = a->alloc(sizeof(char *) * (count + 1));
    for (size_t i = 0; i < count; i++) {
        dst[i] = a->strdup(src[i]);
    }
    dst[count] = NULL;

    return dst;
}

static void free_str_array(char **arr, size_t count) {
    if (!arr) return;

    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

/* ============================================================================
 * Node Creation
 * ============================================================================ */

ASTNode *ast_new_command(void) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_COMMAND,
        .data.command = {0},
    };

    return node;
}

ASTNode *ast_new_pipeline(ASTNode *left, ASTNode *right) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_PIPELINE,
        .data.pipeline = {.left = left, .right = right},
    };

    return node;
}

ASTNode *ast_new_list(ASTNode *left, ASTNode *right, int async) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_LIST,
        .data.list = {.left = left, .right = right, .async = async},
    };

    return node;
}

ASTNode *ast_new_binary(NodeType type, ASTNode *left, ASTNode *right) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = type,
        .data.pipeline = {.left = left, .right = right},
    };

    return node;
}

ASTNode *ast_new_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_IF,
        .data.if_stmt = {
            .condition   = cond,
            .then_branch = then_branch,
            .else_branch = else_branch,
        },
    };

    return node;
}

ASTNode *ast_new_while(ASTNode *cond, ASTNode *body) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_WHILE,
        .data.while_loop = {.condition = cond, .body = body},
    };

    return node;
}

ASTNode *ast_new_until(ASTNode *cond, ASTNode *body) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_UNTIL,
        .data.until_loop = {.condition = cond, .body = body},
    };

    return node;
}

ASTNode *ast_new_for(const char *var, char **words, size_t word_cnt, ASTNode *body) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_FOR,
        .data.for_loop = {
            .var_name   = mem_stack_strdup(var),
            .word_list  = words,
            .word_count = word_cnt,
            .body       = body,
        },
    };

    return node;
}

ASTNode *ast_new_subshell(ASTNode *body) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_SUBSHELL,
        .data.subshell = {.body = body},
    };

    return node;
}

ASTNode *ast_new_group(ASTNode *body) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_GROUP,
        .data.group = {.body = body},
    };

    return node;
}

ASTNode *ast_new_function(const char *name, ASTNode *body) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_FUNCTION,
        .data.function = {
            .name = mem_stack_strdup(name),
            .body = body,
        },
    };

    return node;
}

ASTNode *ast_new_case(const char *word, CaseItem *items, size_t item_cnt) {
    ASTNode *node = mem_stack_alloc(sizeof(ASTNode));

    *node = (ASTNode){
        .type = NODE_CASE,
        .data.case_stmt = {
            .word       = mem_stack_strdup(word),
            .items      = items,
            .item_count = item_cnt,
        },
    };

    return node;
}

/* ============================================================================
 * Command Node Modification
 * ============================================================================ */

void ast_command_add_arg(ASTNode *node, const char *arg) {
    if (node->type != NODE_COMMAND) return;

    CommandNode *cmd = &node->data.command;
    size_t old = cmd->arg_count++;

    cmd->args = mem_stack_realloc_array(
        cmd->args, old, cmd->arg_count + 1, sizeof(char *)
    );
    cmd->args[old]     = mem_stack_strdup(arg);
    cmd->args[old + 1] = NULL;
}

void ast_command_add_assignment(ASTNode *node, const char *name, const char *value) {
    if (node->type != NODE_COMMAND) return;

    CommandNode *cmd = &node->data.command;
    size_t old = cmd->assignment_count++;

    cmd->assignments = mem_stack_realloc_array(
        cmd->assignments, old, cmd->assignment_count, sizeof(Assignment)
    );
    cmd->assignments[old] = (Assignment){
        .name  = mem_stack_strdup(name),
        .value = mem_stack_strdup(value),
    };
}

void ast_command_add_redirection(ASTNode *node, RedirectionType type,
                                  int io_num, const char *file,
                                  const char *heredoc) {
    if (node->type != NODE_COMMAND) return;

    CommandNode *cmd = &node->data.command;
    size_t old = cmd->redirection_count++;

    cmd->redirections = mem_stack_realloc_array(
        cmd->redirections, old, cmd->redirection_count, sizeof(Redirection)
    );
    cmd->redirections[old] = (Redirection){
        .type             = type,
        .io_number        = io_num,
        .filename         = file    ? mem_stack_strdup(file)    : NULL,
        .here_doc_content = heredoc ? mem_stack_strdup(heredoc) : NULL,
    };
}

/* ============================================================================
 * Cloning - Internal Helpers
 * ============================================================================ */

static ASTNode *clone_node(ASTNode *node, const Allocator *a);

static void clone_command(CommandNode *dst, const CommandNode *src, const Allocator *a) {
    dst->arg_count = src->arg_count;
    dst->args      = dup_str_array(a, src->args, src->arg_count);

    dst->redirection_count = src->redirection_count;
    dst->redirections      = NULL;
    if (src->redirections) {
        dst->redirections = a->alloc(sizeof(Redirection) * src->redirection_count);
        for (size_t i = 0; i < src->redirection_count; i++) {
            const Redirection *s = &src->redirections[i];
            dst->redirections[i] = (Redirection){
                .type             = s->type,
                .io_number        = s->io_number,
                .filename         = dup_str(a, s->filename),
                .here_doc_content = dup_str(a, s->here_doc_content),
            };
        }
    }

    dst->assignment_count = src->assignment_count;
    dst->assignments      = NULL;
    if (src->assignments) {
        dst->assignments = a->alloc(sizeof(Assignment) * src->assignment_count);
        for (size_t i = 0; i < src->assignment_count; i++) {
            dst->assignments[i] = (Assignment){
                .name  = a->strdup(src->assignments[i].name),
                .value = a->strdup(src->assignments[i].value),
            };
        }
    }
}

static void clone_case(CaseNode *dst, const CaseNode *src, const Allocator *a) {
    dst->word       = a->strdup(src->word);
    dst->item_count = src->item_count;
    dst->items      = NULL;

    if (!src->items) return;

    dst->items = a->alloc(sizeof(CaseItem) * src->item_count);
    for (size_t i = 0; i < src->item_count; i++) {
        const CaseItem *si = &src->items[i];
        CaseItem *di       = &dst->items[i];

        di->patterns = NULL;
        if (si->patterns) {
            size_t cnt = 0;
            while (si->patterns[cnt]) cnt++;
            di->patterns = dup_str_array(a, si->patterns, cnt);
        }
        di->commands = clone_node(si->commands, a);
    }
}

static ASTNode *clone_node(ASTNode *node, const Allocator *a) {
    if (!node) return NULL;

    ASTNode *n = a->alloc(sizeof(ASTNode));
    n->type = node->type;

    switch (node->type) {
    case NODE_COMMAND:
        clone_command(&n->data.command, &node->data.command, a);
        break;

    case NODE_PIPELINE:
    case NODE_AND:
    case NODE_OR:
        n->data.pipeline.left  = clone_node(node->data.pipeline.left, a);
        n->data.pipeline.right = clone_node(node->data.pipeline.right, a);
        break;

    case NODE_LIST:
        n->data.list.left  = clone_node(node->data.list.left, a);
        n->data.list.right = clone_node(node->data.list.right, a);
        n->data.list.async = node->data.list.async;
        break;

    case NODE_IF:
        n->data.if_stmt.condition   = clone_node(node->data.if_stmt.condition, a);
        n->data.if_stmt.then_branch = clone_node(node->data.if_stmt.then_branch, a);
        n->data.if_stmt.else_branch = clone_node(node->data.if_stmt.else_branch, a);
        break;

    case NODE_WHILE:
        n->data.while_loop.condition = clone_node(node->data.while_loop.condition, a);
        n->data.while_loop.body      = clone_node(node->data.while_loop.body, a);
        break;

    case NODE_UNTIL:
        n->data.until_loop.condition = clone_node(node->data.until_loop.condition, a);
        n->data.until_loop.body      = clone_node(node->data.until_loop.body, a);
        break;

    case NODE_FOR:
        n->data.for_loop.var_name   = a->strdup(node->data.for_loop.var_name);
        n->data.for_loop.word_count = node->data.for_loop.word_count;
        n->data.for_loop.word_list  = dup_str_array(a, node->data.for_loop.word_list,
                                                     node->data.for_loop.word_count);
        n->data.for_loop.body       = clone_node(node->data.for_loop.body, a);
        break;

    case NODE_SUBSHELL:
        n->data.subshell.body = clone_node(node->data.subshell.body, a);
        break;

    case NODE_GROUP:
        n->data.group.body = clone_node(node->data.group.body, a);
        break;

    case NODE_FUNCTION:
        n->data.function.name = a->strdup(node->data.function.name);
        n->data.function.body = clone_node(node->data.function.body, a);
        break;

    case NODE_CASE:
        clone_case(&n->data.case_stmt, &node->data.case_stmt, a);
        break;
    }

    return n;
}

/* ============================================================================
 * Cloning - Public API
 * ============================================================================ */

ASTNode *ast_copy(ASTNode *node) {
    return clone_node(node, &STACK_ALLOC);
}

ASTNode *ast_clone_to_heap(ASTNode *node) {
    return clone_node(node, &HEAP_ALLOC);
}

/* ============================================================================
 * Memory Cleanup
 * ============================================================================ */

void ast_free(ASTNode *node) {
    (void)node; /* Stack allocator: arena reset handles cleanup */
}

static void free_command(CommandNode *cmd) {
    free_str_array(cmd->args, cmd->arg_count);

    for (size_t i = 0; i < cmd->redirection_count; i++) {
        free(cmd->redirections[i].filename);
        free(cmd->redirections[i].here_doc_content);
    }
    free(cmd->redirections);

    for (size_t i = 0; i < cmd->assignment_count; i++) {
        free(cmd->assignments[i].name);
        free(cmd->assignments[i].value);
    }
    free(cmd->assignments);
}

static void free_case(CaseNode *c) {
    free(c->word);

    for (size_t i = 0; i < c->item_count; i++) {
        CaseItem *item = &c->items[i];
        if (item->patterns) {
            for (size_t j = 0; item->patterns[j]; j++) {
                free(item->patterns[j]);
            }
            free(item->patterns);
        }
        ast_free_heap(item->commands);
    }
    free(c->items);
}

void ast_free_heap(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_COMMAND:
        free_command(&node->data.command);
        break;

    case NODE_PIPELINE:
    case NODE_AND:
    case NODE_OR:
        ast_free_heap(node->data.pipeline.left);
        ast_free_heap(node->data.pipeline.right);
        break;

    case NODE_LIST:
        ast_free_heap(node->data.list.left);
        ast_free_heap(node->data.list.right);
        break;

    case NODE_IF:
        ast_free_heap(node->data.if_stmt.condition);
        ast_free_heap(node->data.if_stmt.then_branch);
        ast_free_heap(node->data.if_stmt.else_branch);
        break;

    case NODE_WHILE:
        ast_free_heap(node->data.while_loop.condition);
        ast_free_heap(node->data.while_loop.body);
        break;

    case NODE_UNTIL:
        ast_free_heap(node->data.until_loop.condition);
        ast_free_heap(node->data.until_loop.body);
        break;

    case NODE_FOR:
        free(node->data.for_loop.var_name);
        free_str_array(node->data.for_loop.word_list, node->data.for_loop.word_count);
        ast_free_heap(node->data.for_loop.body);
        break;

    case NODE_SUBSHELL:
        ast_free_heap(node->data.subshell.body);
        break;

    case NODE_GROUP:
        ast_free_heap(node->data.group.body);
        break;

    case NODE_FUNCTION:
        free(node->data.function.name);
        ast_free_heap(node->data.function.body);
        break;

    case NODE_CASE:
        free_case(&node->data.case_stmt);
        break;
    }

    free(node);
}
