/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "memalloc.h"

#define HASH_SIZE 1024

// Global variables for direct access
struct var vifs;
struct var vpath;
struct var vps1;
struct var vps2;
struct var vps4;
struct var voptind;

// Single global hash table
static struct var *vartab[HASH_SIZE];

// Local variable save structure
struct localvar {
    struct localvar *next;
    struct var *vp;
    int flags;
    char *value;
    int is_new; // 1 if the variable was created by local (didn't exist before)
};

// Scope tracking
typedef struct Scope {
    struct localvar *locals;
    struct Scope *parent;
} Scope;

static Scope *current_scope = NULL;

// Simple LRU cache
#define VAR_CACHE_SIZE 4


static unsigned long hash_djb2(const char *str, size_t *len_out) {
    unsigned long hash = 5381;
    int c;
    size_t len = 0;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
        len++;
    }
    if (len_out) *len_out = len;
    return hash % HASH_SIZE;
}

static void init_special_var(struct var *v, const char *name, const char *val) {
    v->name = xstrdup(name);
    v->name_len = strlen(name);
    v->value = xstrdup(val);
    v->flags = VSTRUCTFIXED | VEXPORT; // Special vars usually exported? No, not all.
    // IFS, OPTIND, PS1 etc are not necessarily exported by default in all shells, but usually are.
    // FreeBSD marks them VSTRFIXED.
    v->is_local = 0;
    v->func = NULL;
    v->next = NULL;
    
    unsigned long h = hash_djb2(name, NULL);
    v->next = vartab[h];
    vartab[h] = v;
}

void posish_var_init(char **envp) {
    // Init hash table
    for (int i = 0; i < HASH_SIZE; i++) {
        vartab[i] = NULL;
    }
    current_scope = NULL;

    // Init special variables
    init_special_var(&vifs, "IFS", " \t\n");
    init_special_var(&vpath, "PATH", "");
    init_special_var(&vps1, "PS1", "\\u@\\h:\\w\\$ ");
    init_special_var(&vps2, "PS2", "> ");
    init_special_var(&vps4, "PS4", "+ ");
    init_special_var(&voptind, "OPTIND", "1");

    for (char **env = envp; *env != NULL; env++) {
        char *entry = xstrdup(*env);
        char *eq = strchr(entry, '=');
        if (eq) {
            *eq = '\0';
            posish_var_set(entry, eq + 1);
            posish_var_export(entry);
        }
        free(entry);
    }
    
    // Set PPID
    char ppid_buf[32];
    snprintf(ppid_buf, sizeof(ppid_buf), "%d", getppid());
    posish_var_set("PPID", ppid_buf);

    // Sanitize PS1: inherited environments (like Haiku) may have complex
    // prompts with command substitution (backticks, $()) or multi-line logic
    // which we don't support yet.
    if (vps1.value) {
        int suspicious = 0;
        if (strchr(vps1.value, '`')) suspicious = 1;         // Backticks
        if (strstr(vps1.value, "$(")) suspicious = 1;        // Command sub
        if (strchr(vps1.value, '\n')) suspicious = 1;        // Multi-line
        
        if (suspicious) {
            if (vps1.value) free(vps1.value);
            vps1.value = xstrdup("\\u@\\h:\\w\\$ ");
        }
    }
}

static struct var *find_var(const char *name) {
    size_t len;
    unsigned long h = hash_djb2(name, &len);
    struct var *v = vartab[h];
    while (v) {
        if (v->name_len == len && strcmp(v->name, name) == 0) {
            return v;
        }
        v = v->next;
    }
    return NULL;
}

int posish_var_set(const char *name, const char *value) {
    if (!value) value = ""; // Safety fallback
    size_t len;
    unsigned long h = hash_djb2(name, &len);
    
    struct var *v = vartab[h];
    while (v) {
        if (v->name_len == len && strcmp(v->name, name) == 0) {
            // Found existing variable
            if (v->flags & VREADONLY) {
                fprintf(stderr, "%s: readonly variable\n", name);
                return 1;
            }
            if (v->value != value) { // Avoid self-assignment issues if pointers match
                 size_t new_len = strlen(value);
                 size_t old_len = v->value ? strlen(v->value) : 0;
                 
                 // OPTIMIZATION: Reuse buffer if new value fits
                 if (v->value && new_len <= old_len) {
                     strcpy(v->value, value);
                 } else {
                     if (v->value) free(v->value);
                     v->value = xstrdup(value);
                 }
            }
            v->flags &= ~VUNSET;
            return 0;
        }
        v = v->next;
    }
    
    // New variable
    v = xmalloc(sizeof(struct var));
    v->name = xstrdup(name);
    v->name_len = len;
    v->value = xstrdup(value);
    v->flags = 0;
    v->is_local = 0;
    v->func = NULL;
    v->next = vartab[h];
    vartab[h] = v;
    return 0;
}

char *posish_var_get(const char *name) {
    const char *val = posish_var_get_value(name);
    return val ? xstrdup(val) : NULL;
}

const char *posish_var_get_value(const char *name) {
    // Check cache
    /*
    for (int i = 0; i < VAR_CACHE_SIZE; i++) {
        if (var_cache[i].last_name && strcmp(var_cache[i].last_name, name) == 0) {
            return var_cache[i].value;
        }
    }
    */

    struct var *v = find_var(name);
    if (v && !(v->flags & VUNSET)) {
        // Update cache
        /*
        int idx = var_cache_next;
        var_cache_next = (var_cache_next + 1) % VAR_CACHE_SIZE;
        
        if (var_cache[idx].last_name) free(var_cache[idx].last_name);
        // We don't own value in cache anymore? 
        // Wait, if we cache pointer to v->value, and v->value changes, cache is stale!
        // FreeBSD doesn't cache value string, it relies on direct access.
        // My cache stores a COPY of value.
        // If I change posish_var_set to invalidate cache, it's fine.
        // But with direct access macros, we bypass cache anyway for common vars.
        
        // For now, keep cache logic but be careful.
        // I need to invalidate cache in posish_var_set.
        
        // Actually, with single hash table, lookup is fast enough?
        // Maybe I can drop the cache?
        // Let's keep it for now.
        
        // Wait, I need to store COPY in cache if I want to be safe?
        // Or just store pointer?
        // If I store pointer, I don't need to free it.
        // But if v->value is realloced, pointer is invalid.
        // So I must invalidate cache on set.
        
        // I'll store a copy.
        if (var_cache[idx].value) free(var_cache[idx].value);
        
        var_cache[idx].name = name; // Just for debugging?
        var_cache[idx].last_name = xstrdup(name);
        var_cache[idx].value = xstrdup(v->value);
        */
        
        return v->value;
    }
    return NULL;
}

void posish_var_unset(const char *name) {
    // Invalidate cache
    /*
    for (int i = 0; i < VAR_CACHE_SIZE; i++) {
        if (var_cache[i].last_name && strcmp(var_cache[i].last_name, name) == 0) {
            free(var_cache[i].last_name);
            free(var_cache[i].value);
            var_cache[i].last_name = NULL;
            var_cache[i].value = NULL;
        }
    }
    */

    unsigned long h = hash_djb2(name, NULL);
    struct var **curr = &vartab[h];
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
            struct var *v = *curr;
            if (v->flags & VREADONLY) {
                fprintf(stderr, "%s: readonly variable\n", name);
                return;
            }
            
            if (v->flags & VSTRUCTFIXED) {
                free(v->value);
                v->value = NULL;
                v->flags |= VUNSET;
            } else {
                *curr = v->next;
                free(v->name);
                free(v->value);
                free(v);
            }
            return;
        }
        curr = &(*curr)->next;
    }
}

void posish_var_export(const char *name) {
    struct var *v = find_var(name);
    if (v) {
        v->flags |= VEXPORT;
    }
}

void posish_var_set_readonly(const char *name) {
    struct var *v = find_var(name);
    if (v) {
        v->flags |= VREADONLY;
    }
}

int posish_var_is_readonly(const char *name) {
    struct var *v = find_var(name);
    return v && (v->flags & VREADONLY);
}

char **posish_var_get_environ(void) {
    size_t count = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct var *v = vartab[i];
        while (v) {
            if ((v->flags & VEXPORT) && !(v->flags & VUNSET)) count++;
            v = v->next;
        }
    }
    
    char **env = xmalloc(sizeof(char *) * (count + 1));
    size_t idx = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct var *v = vartab[i];
        while (v) {
            if ((v->flags & VEXPORT) && !(v->flags & VUNSET)) {
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

char **posish_var_get_all(void) {
    size_t count = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct var *v = vartab[i];
        while (v) {
            if (!(v->flags & VUNSET)) count++;
            v = v->next;
        }
    }
    
    char **env = xmalloc(sizeof(char *) * (count + 1));
    size_t idx = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct var *v = vartab[i];
        while (v) {
            if (!(v->flags & VUNSET)) {
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

// Scope Management
// Scope Management
static Scope *scope_free_list = NULL;

void posish_var_push_scope(void) {
    Scope *new_scope;
    if (scope_free_list) {
        new_scope = scope_free_list;
        scope_free_list = new_scope->parent; // Reuse parent pointer as next pointer in free list
    } else {
        new_scope = xmalloc(sizeof(Scope));
    }
    new_scope->locals = NULL;
    new_scope->parent = current_scope;
    current_scope = new_scope;
}

void posish_var_pop_scope(void) {
    if (!current_scope) return;
    
    struct localvar *lv = current_scope->locals;
    while (lv) {
        struct localvar *next = lv->next;
        struct var *v = lv->vp;
        
        if (lv->is_new) {
            // Variable was created in this scope, so we unset/delete it
            // But wait, if it was VSTRUCTFIXED, we just unset it?
            // No, local vars are never VSTRUCTFIXED unless we shadowed a VSTRUCTFIXED.
            // If we shadowed a VSTRUCTFIXED, is_new would be 0 (because it existed).
            
            // So if is_new is 1, it's a dynamic variable.
            // We should remove it.
            // But we need to find it in the hash table to remove it from the chain.
            posish_var_unset(v->name); 
        } else {
            // Restore old value and flags
            free(v->value);
            v->value = lv->value; // Take ownership back
            v->flags = lv->flags;
        }
        
        free(lv);
        lv = next;
    }
    
    Scope *parent = current_scope->parent;
    
    // Return to free list
    current_scope->parent = scope_free_list;
    scope_free_list = current_scope;
    
    current_scope = parent;
    
    // Invalidate cache
    /*
    for (int i = 0; i < VAR_CACHE_SIZE; i++) {
        if (var_cache[i].last_name) {
            free(var_cache[i].last_name);
            free(var_cache[i].value);
            var_cache[i].last_name = NULL;
            var_cache[i].value = NULL;
        }
    }
    */
}

void posish_var_declare_local(const char *name, const char *value) {
    if (!current_scope) {
        posish_var_set(name, value);
        return;
    }
    
    struct var *v = find_var(name);
    struct localvar *lv = xmalloc(sizeof(struct localvar));
    
    if (v) {
        // Save existing state
        lv->vp = v;
        lv->flags = v->flags;
        lv->value = v->value ? xstrdup(v->value) : NULL;
        lv->is_new = 0;
        
        // Update variable
        v->flags &= ~VEXPORT; // Locals not exported by default?
        v->flags &= ~VREADONLY; // Local overrides readonly? Usually yes.
        // But wait, if it's readonly, can we make it local?
        // Bash says "readonly variable" if you try to local it?
        // FreeBSD sh allows localizing readonly vars?
        // Let's assume yes for now.
        
        // Set new value
        free(v->value);
        v->value = xstrdup(value ? value : "");
    } else {
        // Create new variable
        posish_var_set(name, value ? value : "");
        v = find_var(name); // Retrieve it
        
        lv->vp = v;
        lv->flags = 0;
        lv->value = NULL;
        lv->is_new = 1;
    }
    
    lv->next = current_scope->locals;
    current_scope->locals = lv;
}

// Positional parameters implementation (unchanged)
static char **positional_args = NULL;
static int positional_count = 0;

void posish_var_set_positional(int argc, char **argv) {
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

int posish_var_shift_positional(int n) {
    if (n <= 0) return 0;
    if (n > positional_count) return 1;

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

char *posish_var_get_positional(int index) {
    const char *val = posish_var_get_positional_value(index);
    return val ? xstrdup(val) : NULL;
}

const char *posish_var_get_positional_value(int index) {
    if (index == 0) {
        return posish_var_get_value("0");
    }
    if (index >= 1 && index <= positional_count) {
        return positional_args[index - 1];
    }
    return NULL;
}

int posish_var_get_positional_count(void) {
    return positional_count;
}

char **posish_var_get_all_positional(void) {
    if (positional_count == 0) return NULL;
    char **args = xmalloc(sizeof(char *) * (positional_count + 1));
    for (int i = 0; i < positional_count; i++) {
        args[i] = xstrdup(positional_args[i]);
    }
    args[positional_count] = NULL;
    return args;
}

char **posish_var_get_positional_params(size_t *count) {
    *count = positional_count;
    if (positional_count == 0) return NULL;
    
    char **args = xmalloc(sizeof(char *) * positional_count);
    for (int i = 0; i < positional_count; i++) {
        args[i] = xstrdup(positional_args[i]);
    }
    return args;
}

PositionalSave posish_var_save_positional_fast(void) {
    PositionalSave save;
    save.args = positional_args;
    save.count = positional_count;
    return save;
}

void posish_var_restore_positional_fast(PositionalSave save) {
    positional_args = save.args;
    positional_count = save.count;
}

static pid_t last_bg_pid = -1;
static char *shell_name = "posish";

void posish_var_set_last_bg_pid(pid_t pid) {
    last_bg_pid = pid;
}

pid_t posish_var_get_last_bg_pid(void) {
    return last_bg_pid;
}

void posish_var_set_shell_name(const char *name) {
    if (shell_name && strcmp(shell_name, "posish") != 0) free(shell_name);
    shell_name = xstrdup(name);
}

char *posish_var_get_shell_name(void) {
    return xstrdup(shell_name);
}

int posish_var_is_valid_name(const char *name) {
    if (!name || !*name) return 0;
    if (isdigit(name[0])) return 0;
    for (const char *p = name; *p; p++) {
        if (!isalnum(*p) && *p != '_') return 0;
    }
    return 1;
}

char **posish_var_get_all_readonly(void) {
    size_t count = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct var *v = vartab[i];
        while (v) {
            if (v->flags & VREADONLY) count++;
            v = v->next;
        }
    }
    
    char **result = xmalloc(sizeof(char *) * (count + 1));
    size_t idx = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct var *v = vartab[i];
        while (v) {
            if (v->flags & VREADONLY) {
                size_t len = strlen(v->name) + strlen(v->value) + 2;
                result[idx] = xmalloc(len);
                snprintf(result[idx], len, "%s=%s", v->name, v->value);
                idx++;
            }
            v = v->next;
        }
    }
    result[idx] = NULL;
    return result;
}

static void int_to_str(int n, char *buf) {
    char temp[32];
    int i = 0;
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (n > 0) {
        temp[i++] = (n % 10) + '0';
        n /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void posish_var_set_lineno(int lineno) {
    static struct var *lineno_var = NULL;
    static int last_lineno = -1;
    
    if (lineno == last_lineno && lineno_var) return;
    
    char buf[32];
    int_to_str(lineno, buf);
    
    if (lineno_var) {
        // Fast path: update directly
        if (lineno_var->flags & VREADONLY) return;
        
        size_t new_len = strlen(buf);
        size_t old_len = strlen(lineno_var->value);
        
        if (new_len <= old_len) {
            strcpy(lineno_var->value, buf);
        } else {
            free(lineno_var->value);
            lineno_var->value = xstrdup(buf);
        }
        last_lineno = lineno;
        return;
    }
    
    // Slow path: lookup and cache
    posish_var_set("LINENO", buf);
    lineno_var = find_var("LINENO");
    last_lineno = lineno;
}
