# posish User Guide

## Getting Started

### Installation

```bash
cd /path/to/posish
make
sudo make install  # Optional: install to /usr/local/bin
```

### First Steps

**Start interactive shell:**
```bash
./posish
```

**Execute a command:**
```bash
./posish -c 'echo Hello, World!'
```

**Run a script:**
```bash
./posish myscript.sh
```

## Interactive Shell

### Prompt Customization

The `PS1` variable controls your prompt. Special sequences:

- `\u` - Username
- `\h` - Hostname (short)
- `\w` - Current directory (with ~ for home)
- `\$` - `$` for normal user, `#` for root
- `\\` - Literal backslash

**Examples:**
```bash
PS1='$ '                          # Simple
PS1='\u@\h:\w\$ '                 # user@host:/path$
PS1='[\u@\h \w]\$ '               # [user@host /path]$
```

### Command History

- **Up/Down arrows** - Navigate history
- History saved to `~/.sh_history`
- Persistent across sessions

### Line Editing

- **Left/Right arrows** - Move cursor
- **Backspace** - Delete character
- **Ctrl+D** - Exit shell (EOF)

## Variables

### Setting Variables

```bash
# Simple assignment
VAR=value

# With spaces (need quotes)
NAME="John Doe"

# Export to environment
export PATH=/usr/bin:/bin
export VAR=value    # Set and export in one command
```

### Using Variables

```bash
echo $VAR           # Simple expansion
echo ${VAR}         # Explicit expansion (safer)
echo "${VAR}"       # Quoted (preserves whitespace)
```

### Special Variables

```bash
$?      # Exit status of last command
$$      # Current shell PID
$!      # PID of last background job
$0      # Shell/script name
$1-$9   # Positional parameters
$#      # Count of positional parameters
$@      # All parameters (as separate words)
$*      # All parameters (as single word)
$-      # Current shell flags
```

### Parameter Expansion

**Default Values:**
```bash
${VAR:-default}     # Use "default" if VAR is unset/null
${VAR:=default}     # Set VAR to "default" if unset/null
${VAR:?error msg}   # Print error and exit if VAR unset/null
${VAR:+alternate}   # Use "alternate" if VAR is set
```

**Pattern Removal:**
```bash
FILE="path/to/file.txt"

${FILE#*/}          # Remove shortest prefix: "to/file.txt"
${FILE##*/}         # Remove longest prefix: "file.txt" (basename)
${FILE%.*}          # Remove shortest suffix: "path/to/file"
${FILE%%.*}         # Remove longest suffix: "path/to/file"
```

**Practical Examples:**
```bash
# Get filename without path
BASENAME=${FULLPATH##*/}

# Get file extension
EXT=${FILENAME##*.}

# Remove extension
NAMEONLY=${FILENAME%.*}

# Get directory path
DIR=${FULLPATH%/*}
```

## Redirections

### Basic Redirections

```bash
command > file          # Redirect stdout to file (overwrite)
command >> file         # Append stdout to file
command < file          # Read stdin from file
command 2> file         # Redirect stderr to file
command 2>&1            # Redirect stderr to stdout
command > file 2>&1     # Redirect both stdout and stderr
```

### Here-Documents

```bash
cat <<EOF
Line 1
Line 2
Variable: $VAR
EOF
```

**With tab stripping:**
```bash
cat <<-EOF
	This line starts with a tab
	It will be stripped with <<-
EOF
```

### Advanced Redirections

```bash
# Redirect file descriptor 3
exec 3> output.txt
echo "data" >&3
exec 3>&-               # Close fd 3

# Duplicate file descriptors
command 3>&1 1>&2 2>&3  # Swap stdout and stderr
```

## Control Structures

### if Statements

```bash
if [ $VAR = "value" ]; then
    echo "Match"
elif [ $VAR = "other" ]; then
    echo "Other"
else
    echo "No match"
fi
```

**Compact form:**
```bash
if test -f file.txt; then echo "File exists"; fi
```

### Loops

**while:**
```bash
COUNT=1
while [ $COUNT -le 5 ]; do
    echo "Count: $COUNT"
    COUNT=$((COUNT + 1))
done
```

**until:**
```bash
until [ $COUNT -gt 5 ]; do
    echo "Count: $COUNT"
    COUNT=$((COUNT + 1))
done
```

**for:**
```bash
# Iterate over list
for FILE in *.txt; do
    echo "Processing: $FILE"
done

# Iterate over arguments
for ARG in "$@"; do
    echo "Argument: $ARG"
done

# Iterate over positional parameters (default)
for ARG; do
    echo "Arg: $ARG"
done
```

### case Statements

```bash
case $VAR in
    pattern1)
        echo "Matched pattern1"
        ;;
    pattern2|pattern3)
        echo "Matched pattern2 or pattern3"
        ;;
    *)
        echo "No match (default)"
        ;;
esac
```

**Glob patterns supported:**
```bash
case $FILE in
    *.txt)
        echo "Text file"
        ;;
    *.sh)
        echo "Shell script"
        ;;
    [Mm]akefile)
        echo "Makefile"
        ;;
esac
```

## Functions

### Defining Functions

```bash
# Method 1 (POSIX)
myfunc() {
    echo "Hello from function"
}

# Method 2 (with function keyword)
function myfunc {
    echo "Hello from function"
}

# Method 3 (with both)
function myfunc() {
    echo "Hello from function"
}
```

### Function Arguments

```bash
greet() {
    echo "Hello, $1!"
    echo "You passed $# arguments"
    echo "All args: $@"
}

greet Alice Bob
# Output:
# Hello, Alice!
# You passed 2 arguments
# All args: Alice Bob
```

### Return Values

```bash
is_even() {
    if [ $(($1 % 2)) -eq 0 ]; then
        return 0    # Success (true)
    else
        return 1    # Failure (false)
    fi
}

if is_even 4; then
    echo "4 is even"
fi
```

### Local Variables

```bash
myfunc() {
    local VAR="local value"
    echo "Inside: $VAR"
}

VAR="global value"
myfunc
echo "Outside: $VAR"  # Still "global value"
```

## Operators and Commands

### Logical Operators

```bash
# AND - execute second command only if first succeeds
command1 && command2

# OR - execute second command only if first fails  
command1 || command2

# Examples:
mkdir dir && cd dir                    # cd only if mkdir succeeds
test -f file || echo "File not found"  # echo only if test fails
```

### Pipelines

```bash
# Basic pipeline
ls -l | grep ".txt" | wc -l

# Multiple pipes
cat file | sed 's/old/new/g' | sort | uniq
```

### Command Substitution

```bash
# Modern syntax (preferred)
FILES=$(ls *.txt)
DATE=$(date +%Y-%m-%d)

# Backticks (older syntax)
FILES=`ls *.txt`
```

### Arithmetic Expansion

```bash
# Basic arithmetic
RESULT=$((5 + 3))
SUM=$((A + B))
PRODUCT=$((X * Y))

# Operators: + - * / % ( )
COUNT=$((COUNT + 1))
HALF=$((TOTAL / 2))
REMAINDER=$((NUM % 10))
```

## Job Control

### Background Jobs

```bash
# Run in background
sleep 100 &

# Get PID of background job
echo $!

# List jobs
jobs

# Output:
# [1]   Running    sleep 100 &
```

### Job Management

```bash
sleep 100 &         # Start background job
jobs                # List jobs
fg %1               # Bring job 1 to foreground
# Press Ctrl+Z      # Suspend foreground job
bg %1               # Resume job 1 in background
kill %1             # Kill job 1
wait %1             # Wait for job 1 to complete
```

## Signal Handling

### trap Command

```bash
# Trap SIGINT (Ctrl+C)
trap 'echo "Interrupted!"' INT

# Run cleanup on exit
trap 'rm -f /tmp/tempfile' EXIT

# Ignore signal
trap '' INT             # Ignore Ctrl+C

# Reset to default
trap - INT              # Reset SIGINT to default
```

### Signal Names

Common signals:
- `INT` - Interrupt (Ctrl+C)
- `TERM` - Terminate
- `HUP` - Hangup
- `QUIT` - Quit (Ctrl+\)
- `EXIT` - Shell exit (special)
- `USR1`, `USR2` - User-defined signals

## Tips and Tricks

### Debugging Scripts

```bash
# Enable trace mode
set -x
command
set +x

# Or run entire script with trace
posish -x script.sh

# Exit on error
set -e

# Check syntax without execution
posish -n script.sh
```

### Safe Scripts

```bash
#!/usr/bin/env posish
set -euo pipefail  # Exit on error, undefined vars, pipe failures

# Use quotes to prevent word splitting
rm "$FILE"          # Good
rm $FILE            # Bad if FILE contains spaces
```

### Common Patterns

**Check if file exists:**
```bash
if [ -f file.txt ]; then
    echo "File exists"
fi
```

**Loop over files:**
```bash
for file in *.txt; do
    [ -f "$file" ] || continue  # Skip if not a file
    echo "Processing: $file"
done
```

**Read file line by line:**
```bash
while read -r line; do
    echo "Line: $line"
done < file.txt
```

**Parse command options:**
```bash
while getopts "abc:d:" opt; do
    case $opt in
        a) echo "Option -a" ;;
        b) echo "Option -b" ;;
        c) echo "Option -c with arg: $OPTARG" ;;
        d) echo "Option -d with arg: $OPTARG" ;;
        \?) echo "Invalid option" >&2; exit 1 ;;
    esac
done
```

## environment Files

### Login Shell Initialization

When started as login shell (with `-` prefix or `--login`):

1. `/etc/profile` (system-wide)
2. `~/.profile` (user-specific)

### Interactive Non-Login Shell

Sources file specified by `$ENV` variable:

```bash
export ENV="$HOME/.shrc"
```

## Common Issues

### Quote Variables

```bash
# Wrong - word splitting
for file in $FILES; do
    echo $file
done

# Right - preserve spaces
for file in "$FILES"; do
    echo "$file"
done
```

### Test Command

```bash
# Wrong - missing quotes
if [ $VAR = "value" ]; then  # Fails if VAR is empty

# Right - always quote
if [ "$VAR" = "value" ]; then
```

### Arithmetic

```bash
# Wrong - string concatenation
RESULT=$A+$B         # "5+3"

# Right - arithmetic expansion
RESULT=$((A + B))    # 8
```
