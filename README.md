# posish - POSIX-Compliant Shell

**posish** is a lightweight, POSIX-compliant command language interpreter (shell) implementing the IEEE Std 1003.1-2017 specification.

## Features

### ✅ Core Functionality
- **POSIX Compliance**: Implements the POSIX shell command language specification
- **Interactive Mode**: Full command-line editing with history support
- **Script Execution**: Run shell scripts with full POSIX compatibility
- **Job Control**: Background jobs, job management, and process control
- **Signal Handling**: Comprehensive trap support for signal handling

### 🚀 Command-Line Options

```bash
posish [-abCefhimnuvx] [-o option] [-c command_string] [script_file] [arguments...]
```

**Options:**
- `-c` - Execute command string and exit
- `-i` - Force interactive mode
- `-s` - Read commands from standard input
- `-x` - Enable trace mode (print commands before execution)
- `-e` - Exit immediately on error
- `-f` - Disable pathname expansion (globbing)
- `-v` - Verbose mode (print input lines as read)
- `-n` - Syntax check only (don't execute)
- `-u` - Treat unset variables as errors

### 📝 Built-in Commands

posish includes all POSIX-required builtins:

**Core Builtins:**
- `:` - Null command (no-op)
- `.` (dot) - Source script files
- `cd` - Change directory (with `cd -` support)
- `echo` - Print arguments
- `exit` - Exit shell with status
- `export` - Mark variables for export
- `pwd` - Print working directory
- `read` - Read line from input
- `set` - Set shell options and positional parameters
- `shift` - Shift positional parameters
- `unset` - Unset variables or functions

**Control Flow:**
- `break` - Exit from loop
- `continue` - Skip to next loop iteration
- `return` - Return from function

**Job Control:**
- `bg` - Resume jobs in background
- `fg` - Bring jobs to foreground
- `jobs` - List active jobs
- `kill` - Send signals to processes
- `wait` - Wait for job completion

**Advanced:**
- `alias` / `unalias` - Command aliases
- `command` - Bypass function lookup
- `eval` - Evaluate arguments as command
- `exec` - Replace shell with command
- `getopts` - Parse command options
- `local` - Declare local variables
- `printf` - Formatted output
- `readonly` - Mark variables as read-only
- `test` / `[` - Conditional expressions
- `times` - Print process times
- `trap` - Set signal handlers
- `type` - Display command type
- `typeset` - Declare variables with attributes
- `umask` - Set file creation mask

### 🔧 Shell Features

**Variable Expansion:**
```bash
${VAR:-default}     # Use default if unset
${VAR:=default}     # Assign default if unset
${VAR:?message}     # Error if unset
${VAR:+alternate}   # Use alternate if set
${VAR#pattern}      # Remove shortest prefix
${VAR##pattern}     # Remove longest prefix
${VAR%pattern}      # Remove shortest suffix
${VAR%%pattern}     # Remove longest suffix
```

**Special Variables:**
- `$?` - Last exit status
- `$$` - Shell process ID
- `$!` - Last background job PID
- `$0` - Shell/script name
- `$1, $2, ...` - Positional parameters
- `$#` - Number of positional parameters
- `$@` - All positional parameters (separate words)
- `$*` - All positional parameters (single word)
- `$-` - Current shell options

**Expansions:**
- Tilde expansion (`~` → `$HOME`)
- Parameter expansion (`$VAR`, `${VAR}`)
- Command substitution (`$(command)`, `` `command` ``)
- Arithmetic expansion (`$((expression))`)
- Pathname expansion (globbing: `*`, `?`, `[...]`)

**Redirections:**
```bash
>file      # Redirect stdout
>>file     # Append stdout
<file      # Redirect stdin
2>file     # Redirect stderr
2>&1       # Redirect stderr to stdout
<<EOF      # Here-document
<<-EOF     # Here-doc with tab stripping
```

**Control Structures:**
```bash
if condition; then commands; elif condition; then commands; else commands; fi
while condition; do commands; done
until condition; do commands; done
for var in list; do commands; done
case word in pattern) commands;; esac
{ commands; }           # Group commands
( commands )            # Subshell
```

**Operators:**
- `;` - Command separator
- `&` - Background execution
- `&&` - AND (execute if previous succeeds)
- `||` - OR (execute if previous fails)
- `|` - Pipeline

## Installation

```bash
# Clone repository
git clone <repository-url>
cd posish

# Build
make


## Usage Examples

### Basic Usage

```bash
# Interactive mode
./posish

# Execute script
./posish script.sh

# Execute command string
./posish -c 'echo Hello, World!'

# Execute with arguments
./posish -c 'echo $1 $2' shell arg1 arg2

# Trace mode for debugging
./posish -x script.sh
```

### Interactive Features

```bash
# Command history (Up/Down arrows)
# History saved to ~/.sh_history

# Custom prompt (PS1)
export PS1='[\u@\h:\w]\$ '

# Job control
sleep 100 &          # Background job
jobs                 # List jobs
fg %1                # Bring to foreground
bg %1                # Resume in background
```

### Environment Files

**Login shell:**
1. `/etc/profile`
2. `~/.profile`

**Interactive non-login shell:**
- Sources file specified by `$ENV` variable

### Advanced Features

**Functions:**
```bash
myfunc() {
    echo "Arguments: $@"
    echo "Count: $#"
}
myfunc arg1 arg2
```

**Signal Handling:**
```bash
trap 'echo "Interrupted!"; exit' INT
trap 'cleanup' EXIT
```

**Arithmetic:**
```bash
result=$((5 + 3 * 2))
echo $result  # 11
```

## Performance

posish includes several optimizations:
- **Variable lookup caching** - LRU cache reduces hash table lookups
- **Static operator strings** - Reduces heap allocations
- **Fast-path command execution** - Bypasses parser for simple commands
- **Aggressive compiler optimizations** - `-O3 -march=native -flto`

## Compliance

posish implements:
- **IEEE Std 1003.1-2017** (POSIX.1-2017) Shell Command Language
- Tested against POSIX shell compliance test suites
- Near 100% POSIX feature compatibility

**Known Limitations:**
- `LINENO` special variable not implemented
- `PS2` continuation prompt not implemented (requires incomplete command detection)
- Some `set` options accepted but not fully implemented (-e, -f, -v, -n, -u, -a, -m, -b, -C, -h)

## Development

### Project Structure
```
posish/
├── src/               # Source files
│   ├── builtin-cmds/  # Builtin command implementations
│   ├── lexer.c        # Tokenization
│   ├── parser.c       # AST construction
│   ├── executor.c     # Command execution
│   ├── variables.c    # Variable management
│   └── ...
├── include/           # Header files
├── docs/              # Documentation
├── Makefile           # Build configuration
└── README.md          # This file
```

### Building

```bash
make              # Build shell
make clean        # Clean build artifacts
make install      # Install to /usr/local/bin
```

### Testing

```bash
# Run interactive mode
./posish

# Execute test scripts
./posish tests/script.sh

# Enable tracing
./posish -x tests/script.sh
```

## License

GPL-2.0-or-later

## Contributing

Contributions welcome! Please ensure:
- POSIX compliance for all features
- Add tests for new functionality
- Follow existing code style
- Update documentation

## Authors

Copyright © 2025 xaonfc (Mario)

## See Also

- [POSIX Shell Specification](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html)
- `man sh` - Standard shell documentation
- `man bash` - Bourne Again Shell
