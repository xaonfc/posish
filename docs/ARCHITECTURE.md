# Architecture

This document describes the internal architecture of **posish**, focusing on its core components, execution model, and memory management strategies.

## Core Components

The shell is architected as a modular system with distinct phases of operation:

1.  **Lexical Analysis (Lexer)**: Converts raw input streams into a sequence of tokens.
2.  **Syntactic Analysis (Parser)**: Constructs an Abstract Syntax Tree (AST) from the token stream.
3.  **Execution Engine**: Traverses the AST to execute commands.

### Lexer (`src/lexer.c`)
The lexer implements a state machine that handles:
- Tokenization of operators, keywords, and words.
- Quote handling (single, double, backslash).
- Command substitution nesting.
- Alias expansion (during tokenization).

### Parser (`src/parser.c`)
The parser implements a **Recursive Descent** algorithm corresponding to the POSIX shell grammar.
- **AST Construction**: Builds a tree structure representing the command hierarchy.
- **Node Types**: `NODE_COMMAND`, `NODE_PIPELINE`, `NODE_IF`, `NODE_WHILE`, etc.
- **Error Recovery**: Implements synchronization points to recover from syntax errors.

## Execution Engine (`src/executor.c`)

The execution engine uses a **Strategy Pattern** to handle different AST node types.

### Command Execution Flow
1.  **Expansion**: Variable expansion, command substitution, and arithmetic expansion are performed on arguments.
2.  **Redirection**: File descriptors are manipulated to set up I/O streams.
3.  **Dispatch**:
    - **Builtins**: Executed directly within the shell process (zero-copy).
    - **Functions**: Executed by recursively calling the executor on the function body.
    - **External Commands**: Executed via `fork()` + `execve()`.

### `vfork()` Optimization
For command substitutions (`$(...)`), posish employs a critical optimization using `vfork()`:
- **Safety Check**: The AST is analyzed via `is_safe_for_vfork()` to ensure no state modification occurs in the subshell.
- **Execution**: If safe, `vfork()` is used to avoid page table copying, significantly reducing latency.
- **Fallback**: If unsafe (e.g., variable assignment), standard `fork()` is used to ensure process isolation.

## Memory Management

posish employs a hybrid memory management strategy to balance performance and safety.

### 1. Heap Allocation (`xmalloc`)
Used for persistent structures:
- Abstract Syntax Trees (AST)
- Variable storage (Hash Table)
- Function definitions

### 2. Stack Allocator (`src/mem_stack.c`)
An arena-based stack allocator is used for ephemeral data during execution cycles:
- **Purpose**: Rapid allocation/deallocation of temporary strings, expansion results, and path buffers.
- **Mechanism**: A large contiguous block is allocated. Allocations simply bump a pointer.
- **Reset**: The entire stack is reset after each command execution cycle, eliminating fragmentation and individual `free()` overhead.

## Process Model

### Job Control (`src/jobs.c`)
Implements a state machine for managing process groups and terminal control.
- **Foreground/Background**: Manages `tcsetpgrp()` to control terminal access.
- **State Tracking**: Tracks process states (RUNNING, STOPPED, DONE) via `waitpid()` with `WUNTRACED`.

### Signal Handling (`src/signals.c`)
- **Initialization**: Optimized to only register handlers for relevant signals, reducing startup syscalls.
- **Trap System**: Allows users to register custom handlers for signals.
- **Propagation**: Ensures correct signal propagation to child processes.
