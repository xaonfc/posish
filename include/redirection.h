#ifndef REDIRECTION_H
#define REDIRECTION_H

#include <stddef.h>
#include "ast.h"

// Handle a list of redirections
// Returns 0 on success, non-zero on error
int handle_redirections(Redirection *redirs, size_t count);

#endif
