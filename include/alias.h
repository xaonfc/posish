/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef ALIAS_H
#define ALIAS_H

typedef struct Alias {
    char *name;
    char *value;
    struct Alias *next;
} Alias;

void alias_init(void);
void alias_add(const char *name, const char *value);
void alias_remove(const char *name);
char *alias_get(const char *name);
void alias_print_all(void);

#endif
