/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef LINE_EDITOR_H
#define LINE_EDITOR_H

// Read a line with basic editing support
// Returns NULL on EOF or error
// Caller must free the returned string
char *read_line(const char *prompt);

// Initialize history with a file path
void history_init(const char *filename);

// Add a line to history
void history_add(const char *line);

// Get a line from history (0 is most recent)
// Returns NULL if index out of bounds
const char *history_get(int index);

#endif
