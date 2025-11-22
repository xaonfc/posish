/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef MEMALLOC_H
#define MEMALLOC_H

#include <stddef.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

#endif
