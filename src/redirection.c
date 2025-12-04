/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "redirection.h"
#include "buf_output.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int handle_redirections(Redirection *redirs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        Redirection *r = &redirs[i];
        
        // Flush buffered output if redirecting stdout
        if (r->io_number == STDOUT_FILENO) {
            buf_out_flush_all();
        }
        
        int fd = -1;
        int flags = 0;
        int mode = 0666;
        
        if (r->type == REDIR_IN) {
            fd = open(r->filename, O_RDONLY);
            if (fd < 0) {
                error_sys("open: %s", r->filename);
                return 1;
            }
            if (dup2(fd, r->io_number) < 0) {
                error_sys("dup2");
                close(fd);
                return 1;
            }
            close(fd);
        } else if (r->type == REDIR_OUT || r->type == REDIR_OUT_CLOBBER) {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            fd = open(r->filename, flags, mode);
            if (fd < 0) {
                error_sys("open: %s", r->filename);
                return 1;
            }
            if (dup2(fd, r->io_number) < 0) {
                error_sys("dup2");
                close(fd);
                return 1;
            }
            close(fd);
        } else if (r->type == REDIR_APPEND) {
            flags = O_WRONLY | O_CREAT | O_APPEND;
            fd = open(r->filename, flags, mode);
            if (fd < 0) {
                error_sys("open: %s", r->filename);
                return 1;
            }
            if (dup2(fd, r->io_number) < 0) {
                error_sys("dup2");
                close(fd);
                return 1;
            }
            close(fd);
        } else if (r->type == REDIR_IN_DUP || r->type == REDIR_OUT_DUP) {
            int target_fd = atoi(r->filename);
            if (dup2(target_fd, r->io_number) < 0) {
                error_sys("dup2");
                return 1;
            }
        } else if (r->type == REDIR_RDWR) {
            flags = O_RDWR | O_CREAT;
            fd = open(r->filename, flags, mode);
            if (fd < 0) {
                error_sys("open: %s", r->filename);
                return 1;
            }
            if (dup2(fd, r->io_number) < 0) {
                error_sys("dup2");
                close(fd);
                return 1;
            }
            close(fd);
        } else if (r->type == REDIR_HEREDOC || r->type == REDIR_HEREDOC_DASH) {
            char template[] = "/tmp/posish_heredoc_XXXXXX";
            int fd = mkstemp(template);
            if (fd < 0) {
                error_sys("mkstemp");
                return 1;
            }
            unlink(template);
            
            if (r->here_doc_content) {
                ssize_t written = write(fd, r->here_doc_content, strlen(r->here_doc_content));
                (void)written; // Ignore errors in here-doc write for now
            }
            lseek(fd, 0, SEEK_SET);
            
            if (dup2(fd, STDIN_FILENO) < 0) {
                error_sys("dup2");
                close(fd);
                return 1;
            }
            close(fd);
        }
    }
    return 0;
}
