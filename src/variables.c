/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "memalloc.h"

#define HASH_SIZE 1024

typedef struct Variable {
    char *name;
    char *value;
    int exported;
    int is_local;  // 1 if declared with local, 0 otherwise
    int readonly;  // 1 if readonly, 0 otherwise
    struct Variable *next;
} Variable;

typedef struct Scope {
    Variable *vars[HASH_SIZE];  // Local variables in this scope
    struct Scope *parent;        // Parent scope (NULL for global)
} Scope;

static Scope global_scope = {{0}, NULL};
static Scope *current_scope = &global_scope;

// Simple LRU cache for variable lookups (reduces hash lookups)
#define VAR_CACHE_SIZE 4
static struct {
    const char *name;  // Not owned, just pointer comparison
    char *last_name;   // Owned copy for strcmp
    char *value;       // Owned copy of value
    Scope *scope;      // Which scope it was in
} var_cache[VAR_CACHE_SIZE] = {{NULL, NULL, NULL, NULL}};
static int var_cache_next = 0;

static unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % HASH_SIZE;
}

void var_init(char **envp) {
    // Initialize global scope
    for (int i = 0; i < HASH_SIZE; i++) {
        global_scope.vars[i] = NULL;
    }
    current_scope = &global_scope;

    for (char **env = envp; *env != NULL; env++) {
        char *entry = xstrdup(*env);
        char *eq = strchr(entry, '=');
        if (eq) {
            *eq = '\0';
            var_set(entry, eq + 1);
            var_export(entry);
        }
        free(entry);
    }

    // Set default PS1 if not present
    if (!var_get("PS1")) {
        var_set("PS1", "\\u@\\h:\\w\\$ ");
    }
    
    // Set PPID (parent process ID)
    char ppid_buf[32];
    snprintf(ppid_buf, sizeof(ppid_buf), "%d", getppid());
    var_set("PPID", ppid_buf);
    
    // ENV is inherited from environment if set
    // (already loaded from envp above)
}

void var_set(const char *name, const char *value) {
    unsigned long h = hash_djb2(name);
    
    // Invalidate cache for this variable
    for (int i = 0; i < VAR_CACHE_SIZE; i++) {
        if (var_cache[i].last_name && strcmp(var_cache[i].last_name, name) == 0) {
            free(var_cache[i].last_name);
            free(var_cache[i].value);
            var_cache[i].last_name = NULL;
            var_cache[i].value = NULL;
            var_cache[i].name = NULL;
            var_cache[i].scope = NULL;
            break;
        }
    }
    
    // Search in current scope first for local variables
    if (current_scope != &global_scope) {
        Variable *v = current_scope->vars[h];
        while (v) {
            if (strcmp(v->name, name) == 0 && v->is_local) {
                if (v->readonly) {
                    fprintf(stderr, "%s: readonly variable\n", name);
                    return;
                }
                free(v->value);
                v->value = xstrdup(value);
                return;
            }
            v = v->next;
        }
    }
    
    // Search all parent scopes up to global
    Scope *scope = current_scope;
    while (scope) {
        Variable *v = scope->vars[h];
        while (v) {
            if (strcmp(v->name, name) == 0 && !v->is_local) {
                if (v->readonly) {
                    fprintf(stderr, "%s: readonly variable\n", name);
                    return;
                }
                free(v->value);
                v->value = xstrdup(value);
                return;
            }
            v = v->next;
        }
        scope = scope->parent;
    }
    
    // New variable
    Variable *new_var = xmalloc(sizeof(Variable));
    new_var->name = xstrdup(name);
    new_var->value = xstrdup(value);
    new_var->exported = 0;
    new_var->is_local = 0;
    new_var->readonly = 0;
    new_var->next = current_scope->vars[h];
    current_scope->vars[h] = new_var;
}

char *var_get(const char *name) {
    unsigned long h = hash_djb2(name);
    
    // Check cache first
    for (int i = 0; i < VAR_CACHE_SIZE; i++) {
        if (var_cache[i].last_name && strcmp(var_cache[i].last_name, name) == 0) {
            // Cache hit!
            return xstrdup(var_cache[i].value);
        }
    }
    
    // Cache miss - do full lookup
    Scope *scope = current_scope;
    while (scope) {
        Variable *v = scope->vars[h];
        while (v) {
            if (strcmp(v->name, name) == 0) {
                // Found it - cache before returning
                int idx = var_cache_next;
                var_cache_next = (var_cache_next + 1) % VAR_CACHE_SIZE;
                
                // Free old cache entry
                if (var_cache[idx].last_name) free(var_cache[idx].last_name);
                if (var_cache[idx].value) free(var_cache[idx].value);
                
                // Cache new entry
                var_cache[idx].name = name;
                var_cache[idx].last_name = xstrdup(name);
                var_cache[idx].value = xstrdup(v->value);
                var_cache[idx].scope = scope;
                
                return xstrdup(v->value);
            }
            v = v->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

void var_unset(const char *name) {
    unsigned long h = hash_djb2(name);
    
    // Invalidate cache entry if it exists
    for (int i = 0; i < VAR_CACHE_SIZE; i++) {
        if (var_cache[i].last_name && strcmp(var_cache[i].last_name, name) == 0) {
            free(var_cache[i].last_name);
            free(var_cache[i].value);
            var_cache[i].name = NULL;
            var_cache[i].last_name = NULL;
            var_cache[i].value = NULL;
            var_cache[i].scope = NULL;
            // No return here, as the variable might exist in multiple scopes
            // or we need to remove it from the list.
        }
    }

    // Search all scopes
    Scope *scope = current_scope;
    while (scope) {
        Variable **curr = &scope->vars[h];
        while (*curr) {
            if (strcmp((*curr)->name, name) == 0) {
                if ((*curr)->readonly) {
                    fprintf(stderr, "%s: readonly variable\n", name);
                    return;
                }
                Variable *temp = *curr;
                *curr = (*curr)->next;
                free(temp->name);
                free(temp->value);
                free(temp);
                return;
            }
            curr = &(*curr)->next;
        }
        scope = scope->parent;
    }
}

void var_export(const char *name) {
    unsigned long h = hash_djb2(name);
    
    // Search all scopes for the variable to export
    Scope *scope = current_scope;
    while (scope) {
        Variable *v = scope->vars[h];
        while (v) {
            if (strcmp(v->name, name) == 0) {
                v->exported =1;
                return;
            }
            v = v->next;
        }
        scope = scope->parent;
    }
}

char **var_get_environ(void) {
    size_t count = 0;
    
    // Count exported variables (only from global scope)
    for (int i = 0; i < HASH_SIZE; i++) {
        Variable *v = global_scope.vars[i];
        while (v) {
            if (v->exported) count++;
            v = v->next;
        }
    }

    char **env = xmalloc(sizeof(char *) * (count + 1));
    size_t idx = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        Variable *v = global_scope.vars[i];
        while (v) {
            if (v->exported) {
                size_t len = strlen(v->name) + strlen(v->value) + 2;
                env[idx] = xmalloc(len);
                snprintf(env[idx], len, "%s=%s", v->name, v->value);
                idx++;
            }
            v = v->next;
        }
    }
    env[idx] = NULL;
    return env;
}

char **var_get_all(void) {
    size_t count = 0;
    
    // Count all visible variables (from current scope hierarchy)
    Scope *scope = current_scope;
    while (scope) {
        for (int i = 0; i < HASH_SIZE; i++) {
            Variable *v = scope->vars[i];
            while (v) {
                count++;
                v = v->next;
            }
        }
        scope = scope->parent;
    }

    char **env = xmalloc(sizeof(char *) * (count + 1));
    size_t idx = 0;
    scope = current_scope;
    while (scope) {
        for (int i = 0; i < HASH_SIZE; i++) {
            Variable *v = scope->vars[i];
            while (v) {
                size_t len = strlen(v->name) + strlen(v->value) + 2;
                env[idx] = xmalloc(len);
                snprintf(env[idx], len, "%s=%s", v->name, v->value);
                idx++;
                v = v->next;
            }
        }
        scope = scope->parent;
    }
    env[idx] = NULL;
    return env;
}

// Positional Parameters
static char **positional_args = NULL;
static int positional_count = 0;

void var_set_positional(int argc, char **argv) {
    if (positional_args) {
        for (int i = 0; i < positional_count; i++) {
            free(positional_args[i]);
        }
        free(positional_args);
    }
    
    positional_count = argc;
    if (argc > 0) {
        positional_args = xmalloc(sizeof(char *) * argc);
        for (int i = 0; i < argc; i++) {
            positional_args[i] = xstrdup(argv[i]);
        }
    } else {
        positional_args = NULL;
    }
}

int var_shift_positional(int n) {
    if (n <= 0) return 0;
    if (n > positional_count) return 1; // Error

    for (int i = 0; i < n; i++) {
        free(positional_args[i]);
    }

    int new_count = positional_count - n;
    if (new_count > 0) {
        char **new_args = xmalloc(sizeof(char *) * new_count);
        for (int i = 0; i < new_count; i++) {
            new_args[i] = positional_args[i + n];
        }
        free(positional_args);
        positional_args = new_args;
    } else {
        free(positional_args);
        positional_args = NULL;
    }
    positional_count = new_count;
    return 0;
}

char *var_get_positional(int index) {
    if (index >= 1 && index <= positional_count) {
        return xstrdup(positional_args[index - 1]);
    }
    return NULL;
}

int var_get_positional_count(void) {
    return positional_count;
}

char **var_get_all_positional(void) {
    // Return copy of all args
    if (positional_count == 0) return NULL;
    char **args = xmalloc(sizeof(char *) * (positional_count + 1));
    for (int i = 0; i < positional_count; i++) {
        args[i] = xstrdup(positional_args[i]);
    }
    args[positional_count] = NULL;
    return args;
}

char **var_get_positional_params(size_t *count) {
    *count = positional_count;
    if (positional_count == 0) return NULL;
    
    char **args = xmalloc(sizeof(char *) * positional_count);
    for (int i = 0; i < positional_count; i++) {
        args[i] = xstrdup(positional_args[i]);
    }
    return args;
}

static pid_t last_bg_pid = -1;
static char *shell_name = "posish";

void var_set_last_bg_pid(pid_t pid) {
    last_bg_pid = pid;
}

pid_t var_get_last_bg_pid(void) {
    return last_bg_pid;
}

void var_set_shell_name(const char *name) {
    if (shell_name && strcmp(shell_name, "posish") != 0) free(shell_name);
    shell_name = xstrdup(name);
}

char *var_get_shell_name(void) {
    return xstrdup(shell_name);
}

int var_is_valid_name(const char *name) {
    if (!name || !*name) return 0;
    if (isdigit(name[0])) return 0;
    for (const char *p = name; *p; p++) {
        if (!isalnum(*p) && *p != '_') return 0;
    }
    return 1;
}

// Scope management for local variables
void var_push_scope(void) {
    Scope *new_scope = xmalloc(sizeof(Scope));
    for (int i = 0; i < HASH_SIZE; i++) {
        new_scope->vars[i] = NULL;
    }
    new_scope->parent = current_scope;
    current_scope = new_scope;
}

void var_pop_scope(void) {
    if (current_scope == &global_scope) {
        return; // Can't pop global scope
    }
    
    Scope *old_scope = current_scope;
    current_scope = old_scope->parent;
    
    // Free all variables in the old scope
    for (int i = 0; i < HASH_SIZE; i++) {
        Variable *v = old_scope->vars[i];
        while (v) {
            Variable *next = v->next;
            free(v->name);
            free(v->value);
            free(v);
            v = next;
        }
    }
    free(old_scope);
}

void var_set_readonly(const char *name) {
    unsigned long h = hash_djb2(name);
    Scope *scope = current_scope;
    while (scope) {
        Variable *v = scope->vars[h];
        while (v) {
            if (strcmp(v->name, name) == 0) {
                v->readonly = 1;
                return;
            }
            v = v->next;
        }
        scope = scope->parent;
    }
}

// Check if variable is readonly
int var_is_readonly(const char *name) {
    unsigned long h = hash_djb2(name);
    Scope *scope = current_scope;
    while (scope) {
        Variable *v = scope->vars[h];
        while (v) {
            if (strcmp(v->name, name) == 0) {
                return v->readonly;
            }
            v = v->next;
        }
        scope = scope->parent;
    }
    return 0;
}

// Get all readonly variables
char **var_get_all_readonly(void) {
    size_t count = 0;
    size_t capacity = 16;
    char **result = xmalloc(capacity * sizeof(char *));
    
    for (int i = 0; i < HASH_SIZE; i++) {
        Variable *v = global_scope.vars[i];
        while (v) {
            if (v->readonly) {
                if (count >= capacity - 1) {
                    capacity *= 2;
                    result = xrealloc(result, capacity * sizeof(char *));
                }
                size_t len = strlen(v->name) + strlen(v->value) + 2;
                result[count] = xmalloc(len);
                snprintf(result[count], len, "%s=%s", v->name, v->value);
                count++;
            }
            v = v->next;
        }
    }
    result[count] = NULL;
    return result;
}

void var_declare_local(const char *name, const char *value) {
    if (current_scope == &global_scope) {
        // If we're in global scope, just set normally
        var_set(name, value);
        return;
    }
    
    unsigned long h = hash_djb2(name);
    
    // Check if already declared local in current scope
    Variable *v = current_scope->vars[h];
    while (v) {
        if (strcmp(v->name, name) == 0 && v->is_local) {
            free(v->value);
            v->value = xstrdup(value ? value : "");
            return;
        }
        v = v->next;
    }
    
    // Create new local variable in current scope
    v = xmalloc(sizeof(Variable));
    v->name = xstrdup(name);
    v->value = xstrdup(value ? value : "");
    v->exported = 0;
    v->is_local = 1;
    v->readonly = 0;
    v->next = current_scope->vars[h];
    current_scope->vars[h] = v;
}
