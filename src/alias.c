/* SPDX-License-Identifier: GPL-2.0-or-later */


#include "alias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memalloc.h"

static Alias *aliases = NULL;

void alias_init(void) {
    aliases = NULL;
}

void alias_add(const char *name, const char *value) {
    // Check if exists, update
    Alias *a = aliases;
    while (a) {
        if (strcmp(a->name, name) == 0) {
            free(a->value);
            a->value = xstrdup(value);
            return;
        }
        a = a->next;
    }

    // Add new
    a = xmalloc(sizeof(Alias));
    a->name = xstrdup(name);
    a->value = xstrdup(value);
    a->next = aliases;
    aliases = a;
}

void alias_remove(const char *name) {
    Alias *curr = aliases;
    Alias *prev = NULL;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                aliases = curr->next;
            }
            free(curr->name);
            free(curr->value);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

char *alias_get(const char *name) {
    Alias *a = aliases;
    while (a) {
        if (strcmp(a->name, name) == 0) {
            return xstrdup(a->value);
        }
        a = a->next;
    }
    return NULL;
}

void alias_print_all(void) {
    Alias *a = aliases;
    while (a) {
        // Print in format alias name='value'
        // Need to quote value properly? For now simple quoting.
        printf("alias %s='%s'\n", a->name, a->value);
        a = a->next;
    }
}
