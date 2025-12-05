/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "redirection.h"
#include "buf_output.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

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
            if (r->filename[0] == '-' && r->filename[1] == '\0') {
                close(r->io_number);
            } else {
                int target_fd = atoi(r->filename);
                if (dup2(target_fd, r->io_number) < 0) {
                    error_sys("dup2");
                    return 1;
                }
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
            // OPTIMIZATION: Use pipe for small heredocs to avoid disk I/O
            // PIPE_BUF is usually 4096 bytes on Linux
            size_t len = r->here_doc_content ? strlen(r->here_doc_content) : 0;
            
            #ifndef PIPE_BUF
            #define PIPE_BUF 4096
            #endif

            if (len <= PIPE_BUF) {
                int pipefd[2];
                if (pipe(pipefd) < 0) {
                    perror("posish: pipe failed for heredoc");
                    return 1;
                }
                
                if (len > 0) {
                    if (write(pipefd[1], r->here_doc_content, len) < 0) {
                        // Ignore write error
                    }
                }
                close(pipefd[1]); // Close write end
                
                if (dup2(pipefd[0], r->io_number) < 0) { // Use requested FD (default 0)
                     perror("posish: dup2 failed");
                     close(pipefd[0]);
                     return 1;
                }
                close(pipefd[0]);
            } else {
                // Fallback to mkstemp for large heredocs
                char template[] = "/tmp/posish_heredoc_XXXXXX";
                int fd = mkstemp(template);
                if (fd < 0) {
                    perror("posish: mkstemp failed");
                    return 1;
                }
                unlink(template); // Delete file immediately, it stays open

                if (r->here_doc_content) {
                    if (write(fd, r->here_doc_content, len) < 0) {
                        // Ignore write error for now, but at least check it to satisfy compiler
                    }
                }
                lseek(fd, 0, SEEK_SET); // Rewind

                if (dup2(fd, r->io_number) < 0) {
                    perror("posish: dup2 failed");
                    close(fd);
                    return 1;
                }
                close(fd);
            }
        }
    }
    return 0;
}
