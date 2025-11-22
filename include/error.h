/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef ERROR_H
#define ERROR_H

void error_msg(const char *fmt, ...);
void error_sys(const char *fmt, ...);
void error_fatal(const char *fmt, ...);

#endif
