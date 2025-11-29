# Builtin Commands Reference

This document provides a technical reference for all builtin commands implemented in **posish**.

## Core Builtins

### `:` (colon)
Null command. Performs no operation.

- **Syntax**: `:`
- **Exit Status**: Always returns 0.

### `.` (dot)
Executes commands from a file in the current shell environment.

- **Syntax**: `. filename [arguments...]`
- **Exit Status**: Returns the exit status of the last command executed, or 0 if no commands are executed. Returns >0 if file cannot be read.

### `cd`
Changes the current working directory.

- **Syntax**: `cd [-L|-P] [directory]`
- **Options**:
    - `-L`: Use logical path (default).
    - `-P`: Use physical path (resolve symlinks).
    - `-`: Switch to previous directory (`$OLDPWD`).
- **Exit Status**: 0 on success, >0 on error.

### `echo`
Writes arguments to standard output, followed by a newline.

- **Syntax**: `echo [string...]`
- **Behavior**: XSI-compliant (interprets escape sequences like `\n`, `\t`).
- **Exit Status**: Always returns 0.

### `exit`
Terminates the shell execution.

- **Syntax**: `exit [n]`
- **Arguments**: `n` is the exit status (0-255). If omitted, uses the status of the last executed command.
- **Exit Status**: Does not return (terminates process).

### `export`
Marks variables for export to the environment of subsequently executed commands.

- **Syntax**: `export [-p] [name[=value]...]`
- **Options**: `-p` lists all exported variables.
- **Exit Status**: 0 on success.

### `pwd`
Prints the absolute pathname of the current working directory.

- **Syntax**: `pwd [-L|-P]`
- **Options**:
    - `-L`: Logical path (default).
    - `-P`: Physical path.
- **Exit Status**: 0 on success, >0 on error.

### `read`
Reads a line from standard input and assigns fields to variables.

- **Syntax**: `read [-r] var...`
- **Options**: `-r` disables backslash escape processing (raw mode).
- **Exit Status**: 0 on success, >0 on EOF or error.

### `unset`
Unsets values and attributes of variables and functions.

- **Syntax**: `unset [-v|-f] name...`
- **Options**:
    - `-v`: Unset variable (default).
    - `-f`: Unset function.
- **Exit Status**: 0 on success.

## Control Flow

### `break`
Exits from a `for`, `while`, `until`, or `case` loop.

- **Syntax**: `break [n]`
- **Arguments**: `n` is the number of nested loops to break out of (default 1).
- **Exit Status**: 0 on success.

### `continue`
Resumes the next iteration of a `for`, `while`, `until`, or `case` loop.

- **Syntax**: `continue [n]`
- **Arguments**: `n` is the number of nested loops to resume (default 1).
- **Exit Status**: 0 on success.

### `return`
Returns from a function or dot script.

- **Syntax**: `return [n]`
- **Arguments**: `n` is the return status. If omitted, uses the status of the last executed command.
- **Exit Status**: Returns `n`.

## Job Control

### `bg`
Resumes suspended jobs in the background.

- **Syntax**: `bg [jobspec...]`
- **Exit Status**: 0 on success.

### `fg`
Moves jobs to the foreground.

- **Syntax**: `fg [jobspec]`
- **Exit Status**: Returns the exit status of the command placed in the foreground.

### `jobs`
Lists active jobs.

- **Syntax**: `jobs [-l|-p] [jobspec...]`
- **Exit Status**: 0 on success.

### `kill`
Sends a signal to a process or job.

- **Syntax**:
    - `kill [-s sigspec | -n signum | -sigspec] pid|jobspec...`
    - `kill -l [exit_status]`
- **Exit Status**: 0 if at least one signal was sent successfully.

### `wait`
Waits for the specified process or job to complete and returns its exit status.

- **Syntax**: `wait [pid|jobspec...]`
- **Exit Status**: Returns the exit status of the waited-for command.

## Utility Commands

### `alias`
Defines or displays aliases.

- **Syntax**: `alias [name[=value]...]`
- **Exit Status**: 0 on success.

### `command`
Executes a simple command, suppressing shell function lookup.

- **Syntax**: `command [-p] [-v|-V] command [arg...]`
- **Exit Status**: Returns the exit status of `command`.

### `eval`
Constructs a command by concatenating arguments and executes it.

- **Syntax**: `eval [arg...]`
- **Exit Status**: Returns the exit status of the executed command.

### `exec`
Replaces the shell process with the specified command, or modifies file descriptors.

- **Syntax**: `exec [-c] [command [arg...]]`
- **Exit Status**: Does not return if command is executed. Returns 0 if only redirections are performed.

### `getopts`
Parses positional parameters for options.

- **Syntax**: `getopts optstring name [arg...]`
- **Exit Status**: 0 if an option is found, >0 if end of options.

### `printf`
Writes formatted output.

- **Syntax**: `printf format [argument...]`
- **Exit Status**: 0 on success, >0 on error.

### `set`
Sets or unsets shell options and positional parameters.

- **Syntax**: `set [-abCefhimnuvx] [-o option] [arg...]`
- **Exit Status**: 0 on success.

### `shift`
Shifts positional parameters to the left.

- **Syntax**: `shift [n]`
- **Exit Status**: 0 on success, >0 if `n` exceeds `$#`.

### `test` / `[`
Evaluates conditional expressions.

- **Syntax**: `test expression` or `[ expression ]`
- **Exit Status**: 0 if expression is true, 1 if false, >1 on error.

### `times`
Prints the accumulated user and system times for the shell and its children.

- **Syntax**: `times`
- **Exit Status**: 0.

### `trap`
Sets action to be taken on receipt of a signal.

- **Syntax**: `trap [action condition...]`
- **Exit Status**: 0 on success.

### `true`
Does nothing, successfully.

- **Syntax**: `true`
- **Exit Status**: Always returns 0.

### `type`
Indicates how each name would be interpreted if used as a command name.

- **Syntax**: `type name...`
- **Exit Status**: 0 if all names are found, >0 if any are not found.

### `umask`
Sets the file mode creation mask.

- **Syntax**: `umask [-S] [mask]`
- **Exit Status**: 0 on success.

### `unalias`
Removes alias definitions.

- **Syntax**: `unalias name...` or `unalias -a`
- **Exit Status**: 0 on success.
