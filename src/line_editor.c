/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include "memalloc.h"
#include "line_editor.h"
#include "input.h"
#include "output.h"

#define BUFFER_SIZE 1024
#define HISTORY_SIZE 100

static struct termios orig_termios;
static int raw_mode_enabled = 0;

static char *history[HISTORY_SIZE];
static int history_count = 0;
static char *history_file = NULL;

void history_init(const char *filename) {
    if (history_file) free(history_file);
    history_file = xstrdup(filename);
    
    FILE *f = fopen(history_file, "r");
    if (!f) return;
    
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        // Remove newline
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n') line[l-1] = '\0';
        
        if (history_count < HISTORY_SIZE) {
            history[history_count++] = xstrdup(line);
        } else {
            free(history[0]);
            memmove(history, history + 1, (HISTORY_SIZE - 1) * sizeof(char*));
            history[HISTORY_SIZE - 1] = xstrdup(line);
        }
    }
    free(line);
    fclose(f);
}

void history_add(const char *line) {
    if (!line || !*line) return;
    
    // Create a copy to strip newline
    char *line_copy = xstrdup(line);
    size_t len = strlen(line_copy);
    if (len > 0 && line_copy[len - 1] == '\n') {
        line_copy[len - 1] = '\0';
        len--;
    }
    
    if (len == 0) {
        free(line_copy);
        return;
    }
    
    // Don't add duplicates of the last command
    if (history_count > 0 && strcmp(history[history_count - 1], line_copy) == 0) {
        free(line_copy);
        return;
    }
    
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = line_copy;
    } else {
        free(history[0]);
        memmove(history, history + 1, (HISTORY_SIZE - 1) * sizeof(char*));
        history[HISTORY_SIZE - 1] = line_copy;
    }
    
    if (history_file) {
        FILE *f = fopen(history_file, "a");
        if (f) {
            fprintf(f, "%s\n", line_copy);
            fclose(f);
        }
    }
}

const char *history_get(int index) {
    if (index < 0 || index >= history_count) return NULL;
    return history[index];
}

// Enable raw mode
static void enable_raw_mode(void) {
    if (raw_mode_enabled) return;
    if (!input_is_tty()) return;
    
    tcgetattr(input_get_fd(), &orig_termios);
    struct termios raw = orig_termios;
    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(input_get_fd(), TCSAFLUSH, &raw);
    raw_mode_enabled = 1;
}

// Disable raw mode
static void disable_raw_mode(void) {
    if (!raw_mode_enabled) return;
    tcsetattr(input_get_fd(), TCSAFLUSH, &orig_termios);
    
    // Also restore stdout and stderr if they're TTYs
    if (output_is_tty()) {
        tcsetattr(output_get_fd(), TCSAFLUSH, &orig_termios);
    }
    if (error_is_tty()) {
        tcsetattr(error_get_fd(), TCSAFLUSH, &orig_termios);
    }
    
    raw_mode_enabled = 0;
}

// Read a single character
static int read_char(void) {
    char c;
    if (input_read_char(&c) != -1) {
        return (unsigned char)c;
    }
    return -1;
}

// Read escape sequence
static int read_escape_seq(void) {
    char seq[3];
    
    if (input_read_char(&seq[0]) == -1) return -1;
    if (input_read_char(&seq[1]) == -1) return -1;
    
    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            if (input_read_char(&seq[2]) == -1) return -1;
            if (seq[2] == '~') {
                switch (seq[1]) {
                    case '1': return 1000; // Home
                    case '3': return 1001; // Delete
                    case '4': return 1002; // End
                    case '7': return 1000; // Home (alternate)
                    case '8': return 1002; // End (alternate)
                }
            }
        } else {
            switch (seq[1]) {
                case 'A': return 1003; // Up
                case 'B': return 1004; // Down
                case 'C': return 1005; // Right
                case 'D': return 1006; // Left
                case 'H': return 1000; // Home
                case 'F': return 1002; // End
            }
        }
    } else if (seq[0] == 'O') {
        switch (seq[1]) {
            case 'H': return 1000; // Home
            case 'F': return 1002; // End
        }
    }
    
    return -1;
}

// Refresh the line display
static void refresh_line(const char *prompt, const char *buf, size_t len, size_t pos) {
    // Move cursor to start of line, clear it, and redraw
    output_write("\r", 1);
    output_write("\x1b[K", 3); // Clear to end of line
    output_write(prompt, strlen(prompt));
    output_write(buf, len);
    
    // Move cursor to correct position
    size_t cursor_pos = strlen(prompt) + pos;
    char seq[32];
    snprintf(seq, sizeof(seq), "\r\x1b[%zuC", cursor_pos);
    output_write(seq, strlen(seq));
}

char *read_line(const char *prompt) {
    if (!input_is_tty()) {
        // Non-interactive: use getline
        // If prompt is provided (interactive mode but non-TTY input), print it to stderr
        if (prompt) {
            error_write(prompt, strlen(prompt));
            error_flush();
        }
        
        char *line = NULL;
        size_t len = 0;
        if (input_getline(&line, &len) == -1) {
            free(line);
            return NULL;
        }
        return line;
    }
    
    enable_raw_mode();
    
    char *buf = xmalloc(BUFFER_SIZE);
    if (!buf) {
        disable_raw_mode();
        return NULL;
    }
    
    size_t len = 0;
    size_t pos = 0;
    size_t capacity = BUFFER_SIZE;
    buf[0] = '\0';
    
    int history_index = history_count;
    
    // Display initial prompt
    output_write(prompt, strlen(prompt));
    
    while (1) {
        int c = read_char();
        if (c == -1) {
            disable_raw_mode();
            free(buf);
            return NULL;
        }
        
        if (c == 27) { // ESC
            int key = read_escape_seq();
            if (key == 1003) { // Up arrow
                if (history_index > 0) {
                    history_index--;
                    const char *hist_line = history_get(history_index);
                    if (hist_line) {
                        size_t hist_len = strlen(hist_line);
                        if (hist_len < capacity) {
                            strcpy(buf, hist_line);
                            len = hist_len;
                            pos = hist_len;
                            refresh_line(prompt, buf, len, pos);
                        }
                    }
                }
            } else if (key == 1004) { // Down arrow
                if (history_index < history_count) {
                    history_index++;
                    if (history_index == history_count) {
                        // New empty line
                        buf[0] = '\0';
                        len = 0;
                        pos = 0;
                        refresh_line(prompt, buf, len, pos);
                    } else {
                        const char *hist_line = history_get(history_index);
                        if (hist_line) {
                            size_t hist_len = strlen(hist_line);
                            if (hist_len < capacity) {
                                strcpy(buf, hist_line);
                                len = hist_len;
                                pos = hist_len;
                                refresh_line(prompt, buf, len, pos);
                            }
                        }
                    }
                }
            } else if (key == 1003) { // Up arrow
                if (history_index > 0) {
                    history_index--;
                    const char *hist_line = history_get(history_index);
                    if (hist_line) {
                        size_t hist_len = strlen(hist_line);
                        if (hist_len < capacity) {
                            strcpy(buf, hist_line);
                            len = hist_len;
                            pos = hist_len;
                            refresh_line(prompt, buf, len, pos);
                        }
                    }
                }
            } else if (key == 1004) { // Down arrow
                if (history_index < history_count) {
                    history_index++;
                    if (history_index == history_count) {
                        // New empty line
                        buf[0] = '\0';
                        len = 0;
                        pos = 0;
                        refresh_line(prompt, buf, len, pos);
                    } else {
                        const char *hist_line = history_get(history_index);
                        if (hist_line) {
                            size_t hist_len = strlen(hist_line);
                            if (hist_len < capacity) {
                                strcpy(buf, hist_line);
                                len = hist_len;
                                pos = hist_len;
                                refresh_line(prompt, buf, len, pos);
                            }
                        }
                    }
                }
            } else if (key == 1006) { // Left arrow
                if (pos > 0) {
                    pos--;
                    refresh_line(prompt, buf, len, pos);
                }
            } else if (key == 1005) { // Right arrow
                if (pos < len) {
                    pos++;
                    refresh_line(prompt, buf, len, pos);
                }
            } else if (key == 1000) { // Home
                pos = 0;
                refresh_line(prompt, buf, len, pos);
            } else if (key == 1002) { // End
                pos = len;
                refresh_line(prompt, buf, len, pos);
            } else if (key == 1001) { // Delete
                if (pos < len) {
                    memmove(buf + pos, buf + pos + 1, len - pos);
                    len--;
                    buf[len] = '\0';
                    refresh_line(prompt, buf, len, pos);
                }
            }
        } else if (c == 127 || c == 8) { // Backspace
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos + 1);
                pos--;
                len--;
                refresh_line(prompt, buf, len, pos);
            }
        } else if (c == 4) { // Ctrl+D (EOF)
            if (len == 0) {
                disable_raw_mode();
                free(buf);
                output_write("\n", 1);
                return NULL;
            }
        } else if (c == 3) { // Ctrl+C
            disable_raw_mode();
            output_write("^C\n", 3);
            buf[0] = '\0';
            len = 0;
            pos = 0;
            enable_raw_mode();
            output_write(prompt, strlen(prompt));
        } else if (c == '\r' || c == '\n') { // Enter
            disable_raw_mode();
            output_write("\r\n", 2);
            buf[len] = '\n';
            buf[len + 1] = '\0';
            return buf;
        } else if (c >= 32 && c < 127) { // Printable character
            // Expand buffer if needed
            if (len + 2 >= capacity) {
                capacity *= 2;
                char *new_buf = xrealloc(buf, capacity);
                if (!new_buf) {
                    disable_raw_mode();
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }
            
            // Insert character
            memmove(buf + pos + 1, buf + pos, len - pos + 1);
            buf[pos] = c;
            pos++;
            len++;
            buf[len] = '\0';
            refresh_line(prompt, buf, len, pos);
        }
    }
}
