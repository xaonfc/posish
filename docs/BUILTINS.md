# posish Builtin Commands Reference

Complete reference for all builtin commands in posish.

## Table of Contents

- [Core Builtins](#core-builtins)
- [Control Flow](#control-flow)
- [Job Control](#job-control)
- [Variables & Functions](#variables--functions)
- [Utility Commands](#utility-commands)

---

## Core Builtins

### `:` (colon)
Null command. Does nothing, returns success (0).

**Usage:**
```bash
:
```

**Example:**
```bash
: ${VAR:=default}  # Set VAR if unset, discard output
while :; do        # Infinite loop
    echo "Running..."
done
```

---

### `.` (dot)
Source/execute commands from a file in the current shell environment.

**Usage:**
```bash
. filename [arguments...]
```

**Example:**
```bash
. ~/.profile
. ./functions.sh
. config.sh arg1 arg2
```

---

### cd
Change the current working directory.

**Usage:**
```bash
cd [directory]
cd -
```

**Options:**
- No argument: Change to `$HOME`
- `-`: Change to previous directory (`$OLDPWD`)

**Example:**
```bash
cd /tmp
cd ..
cd ~user
cd -           # Toggle between current and previous dir
```

---

### echo
Write arguments to standard output.

**Usage:**
```bash
echo [string...]
```

**Example:**
```bash
echo Hello, World!
echo "Line 1" "Line 2"
echo $VAR
```

---

### exit
Exit the shell with specified status.

**Usage:**
```bash
exit [n]
```

**Options:**
- `n`: Exit status (0-255), defaults to status of last command

**Example:**
```bash
exit 0
exit 1
exit $?
```

---

### export
Mark variables for export to child processes.

**Usage:**
```bash
export [name[=value]...]
```

**Example:**
```bash
export PATH=/usr/bin:/bin
export EDITOR=vim
VAR=value; export VAR
export -p              # List all exported variables
```

---

### pwd
Print the current working directory.

**Usage:**
```bash
pwd [-L|-P]
```

**Options:**
- `-L`: Logical path (default, uses `$PWD`)
- `-P`: Physical path (resolve symlinks)

**Example:**
```bash
pwd
pwd -P
```

---

### read
Read a line from standard input into variables.

**Usage:**
```bash
read [-r] var...
```

**Options:**
- `-r`: Raw mode (don't treat backslash as escape)

**Example:**
```bash
read NAME
echo "Hello, $NAME"

read -r LINE          # Don't interpret backslashes
while read -r line; do
    echo "$line"
done < file.txt
```

---

### unset
Unset variables or functions.

**Usage:**
```bash
unset [-v|-f] name...
```

**Options:**
- `-v`: Unset variable (default)
- `-f`: Unset function

**Example:**
```bash
unset VAR
unset -f function_name
```

---

## Control Flow

### break
Exit from `for`, `while`, `until`, or `case` loop.

**Usage:**
```bash
break [n]
```

**Options:**
- `n`: Exit from `n` enclosing loops (default: 1)

**Example:**
```bash
while true; do
    if [ condition ]; then
        break
    fi
done
```

---

### continue
Skip to next iteration of loop.

**Usage:**
```bash
continue [n]
```

**Options:**
- `n`: Skip `n` enclosing loops (default: 1)

**Example:**
```bash
for file in *; do
    [ -f "$file" ] || continue
    echo "Processing $file"
done
```

---

### return
Return from a shell function.

**Usage:**
```bash
return [n]
```

**Options:**
- `n`: Return status (0-255), defaults to status of last command

**Example:**
```bash
myfunc() {
    [ $# -eq 0 ] && return 1
    echo "Processing $1"
    return 0
}
```

---

## Job Control

### bg
Resume suspended jobs in the background.

**Usage:**
```bash
bg [jobspec...]
```

**Example:**
```bash
bg %1
bg %2 %3
```

---

### fg
Bring jobs to the foreground.

**Usage:**
```bash
fg [jobspec]
```

**Example:**
```bash
fg %1
fg %%           # Current job
```

---

### jobs
List active jobs.

**Usage:**
```bash
jobs [jobspec...]
```

**Example:**
```bash
jobs
# [1]   Running    sleep 100 &
# [2] Running    ./long_task &
```

---

### kill
Send signal to processes or jobs.

**Usage:**
```bash
kill [-s sigspec | -n signum | -sigspec] pid|jobspec...
kill -l [exit_status]
```

**Options:**
- `-s sigspec`: Send signal by name
- `-n signum`: Send signal by number
- `-sigspec`: Send signal (e.g., `-TERM`)
- `-l`: List signal names

**Example:**
```bash
kill 1234
kill %1
kill -9 1234
kill -TERM %1
kill -l          # List signals
```

---

### wait
Wait for job completion.

**Usage:**
```bash
wait [pid|jobspec...]
```

**Example:**
```bash
sleep 10 &
PID=$!
wait $PID
echo "Done"

# Wait for all background jobs
wait
```

---

## Variables & Functions

### alias
Define or display command aliases.

**Usage:**
```bash
alias [name[=value]...]
```

**Example:**
```bash
alias ll='ls -l'
alias la='ls -la'
alias                # List all aliases
```

---

### command
Execute command bypassing function lookup.

**Usage:**
```bash
command [-p] [-v|-V] command [arguments...]
```

**Options:**
- `-p`: Use default PATH
- `-v`: Print pathname of command
- `-V`: Verbose description

**Example:**
```bash
command ls          # Run /bin/ls, not function
command -v ls       # Show path to ls
```

---

### eval
Evaluate arguments as shell command.

**Usage:**
```bash
eval [argument...]
```

**Example:**
```bash
CMD='echo hello'
eval "$CMD"

VAR=MY_VAR
eval echo \$$VAR     # Echo value of $MY_VAR
```

---

### exec
Replace shell with command, or modify file descriptors.

**Usage:**
```bash
exec [-c] [command [arguments...]]
```

**Example:**
```bash
exec ls             # Replace shell with ls
exec 2>error.log   # Redirect stderr for rest of script
exec 3<input.txt    # Open file descriptor 3
```

---

### getopts
Parse command options.

**Usage:**
```bash
getopts optstring name [args...]
```

**Example:**
```bash
while getopts "abc:" opt; do
    case $opt in
        a) echo "Option -a" ;;
        b) echo "Option -b" ;;
        c) echo "Option -c: $OPTARG" ;;
        \?) echo "Invalid option" >&2; exit 1 ;;
    esac
done
```

---

### local
Declare local variables in functions.

**Usage:**
```bash
local [name[=value]...]
```

**Example:**
```bash
myfunc() {
    local VAR="local value"
    echo "$VAR"
}
```

---

### readonly
Mark variables as read-only.

**Usage:**
```bash
readonly [name[=value]...]
```

**Example:**
```bash
readonly PI=3.14159
readonly HOSTNAME
readonly -p         # List readonly variables
```

---

### set
Set or unset shell options and positional parameters.

**Usage:**
```bash
set [--] [arguments...]
set [-+]eoption

```

**Options:**
- `-x`: Enable trace mode
- `+x`: Disable trace mode
- `-e`: Exit on error
- `-v`: Verbose mode
- `--`: End options, set positional parameters

**Example:**
```bash
set -x              # Enable trace
set +x              # Disable trace
set -- arg1 arg2    # Set $1=arg1, $2=arg2
set                 # List all variables
```

---

### shift
Shift positional parameters.

**Usage:**
```bash
shift [n]
```

**Options:**
- `n`: Shift by `n` positions (default: 1)

**Example:**
```bash
# $1=a, $2=b, $3=c
shift
# Now $1=b, $2=c
```

---

### typeset
Declare variables with attributes (compatibility alias).

**Usage:**
```bash
typeset [name[=value]...]
```

**Example:**
```bash
typeset VAR=value
```

---

### unalias
Remove alias definitions.

**Usage:**
```bash
unalias name...
```

**Example:**
```bash
unalias ll
unalias -a          # Remove all aliases
```

---

## Utility Commands

### false
Return failure status (1).

**Usage:**
```bash
false
```

**Example:**
```bash
false || echo "This runs"
```

---

### printf
Format and print data.

**Usage:**
```bash
printf format [arguments...]
```

**Format Specifiers:**
- `%s`: String
- `%d`, `%i`: Integer
- `%o`: Octal
- `%x`, `%X`: Hexadecimal
- `%c`: Character
- `%b`: String with backslash escapes
- `%%`: Literal %

**Example:**
```bash
printf "Hello, %s!\n" "World"
printf "%d + %d = %d\n" 5 3 8
printf "%x\n" 255        # ff
printf "%b\n" "Line1\nLine2"
```

---

### test / [
Evaluate conditional expressions.

**Usage:**
```bash
test expression
[ expression ]
```

**File Tests:**
- `-f file`: File exists and is regular file
- `-d file`: File exists and is directory
- `-e file`: File exists
- `-r file`: File is readable
- `-w file`: File is writable  
- `-x file`: File is executable
- `-s file`: File exists and has size > 0

**String Tests:**
- `-z string`: String is empty
- `-n string`: String is not empty
- `s1 = s2`: Strings are equal
- `s1 != s2`: Strings are not equal

**Numeric Tests:**
- `n1 -eq n2`: Equal
- `n1 -ne n2`: Not equal
- `n1 -lt n2`: Less than
- `n1 -le n2`: Less than or equal
- `n1 -gt n2`: Greater than
- `n1 -ge n2`: Greater than or equal

**Example:**
```bash
if [ -f file.txt ]; then echo "File exists"; fi
if [ "$VAR" = "value" ]; then echo "Match"; fi
if [ $NUM -gt 10 ]; then echo "Greater than 10"; fi
```

---

### times
Print accumulated process times.

**Usage:**
```bash
times
```

**Example:**
```bash
times
# 0m0.01s 0m0.01s
# 0m0.05s 0m0.03s
# (user and system time for shell and children)
```

---

### trap
Set signal handlers.

**Usage:**
```bash
trap [-l] [action condition...]
```

**Options:**
- `-l`: List signal names

**Example:**
```bash
trap 'echo Interrupted' INT
trap 'cleanup; exit' EXIT
trap - INT              # Reset INT to default
trap '' INT             # Ignore INT
trap -l                 # List signals
```

---

### true
Return success status (0).

**Usage:**
```bash
true
```

**Example:**
```bash
while true; do
    echo "Infinite loop"
done
```

---

### type
Display command type information.

**Usage:**
```bash
type name...
```

**Example:**
```bash
type ls      # ls is /bin/ls
type cd      # cd is a shell builtin
type myfunc  # myfunc is a function
```

---

### umask
Set file creation mask.

**Usage:**
```bash
umask [-S] [mask]
```

**Options:**
- `-S`: Symbolic output

**Example:**
```bash
umask
umask 022
umask u=rwx,g=rx,o=rx
umask -S
```
