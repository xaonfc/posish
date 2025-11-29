/* SPDX-License-Identifier: GPL-2.0-or-later */


#ifndef VARIABLES_H
#define VARIABLES_H

#include <sys/types.h>

/* Variable flags */
#define VEXPORT     0x01    /* variable is exported */
#define VREADONLY   0x02    /* variable cannot be modified */
#define VTEXTFIXED  0x04    /* text is statically allocated */
#define VSTACK      0x08    /* text is allocated on the stack */
#define VUNSET      0x10    /* the variable is not set */
#define VNOFUNC     0x20    /* don't call the callback function */
#define VSTRUCTFIXED 0x40   /* variable struct is statically allocated */

struct var {
    struct var *next;       /* next entry in hash list */
    char *name;             /* variable name */
    char *value;            /* variable value */
    int flags;              /* flags defined above */
    int is_local;           /* 1 if declared with local */
    void (*func)(const char *); /* function to be called when set/unset */
};

/* Common variables for direct access */
extern struct var vifs;
extern struct var vpath;
extern struct var vps1;
extern struct var vps2;
extern struct var vps4;
extern struct var voptind;

/* Macros for direct access */
#define ifsval()    (vifs.value ? vifs.value : " \t\n")
#define pathval()   (vpath.value ? vpath.value : "")
#define ps1val()    (vps1.value ? vps1.value : "")
#define ps2val()    (vps2.value ? vps2.value : "")
#define ps4val()    (vps4.value ? vps4.value : "")
#define optindval() (voptind.value ? voptind.value : "1")

void posish_var_init(char **envp);
int posish_var_set(const char *name, const char *value);
char *posish_var_get(const char *name);
const char *posish_var_get_value(const char *name);
void posish_var_unset(const char *name);
void posish_var_export(const char *name);
char **posish_var_get_environ(void);
char **posish_var_get_all(void);
int posish_var_is_valid_name(const char *name);
void posish_var_set_readonly(const char *name);
int posish_var_is_readonly(const char *name);
char **posish_var_get_all_readonly(void);

void posish_var_set_positional(int argc, char **argv);
char *posish_var_get_positional(int index);
const char *posish_var_get_positional_value(int index);
int posish_var_get_positional_count(void);
char **posish_var_get_all_positional(void);
int posish_var_shift_positional(int n);
char **posish_var_get_positional_params(size_t *count);

typedef struct {
    char **args;
    int count;
} PositionalSave;

PositionalSave posish_var_save_positional_fast(void);
void posish_var_restore_positional_fast(PositionalSave save);


void posish_var_set_last_bg_pid(pid_t pid);
pid_t posish_var_get_last_bg_pid(void);
void posish_var_set_shell_name(const char *name);
char *posish_var_get_shell_name(void);

void posish_var_push_scope(void);
void posish_var_pop_scope(void);
void posish_var_declare_local(const char *name, const char *value);

#endif
