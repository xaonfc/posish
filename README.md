# posish

**posish** is a high-performance, strictly POSIX-compliant command language interpreter implementing the IEEE Std 1003.1-2017 specification. Designed for correctness, speed, and minimal resource footprint, it serves as a drop-in replacement for `/bin/sh` in embedded systems and performance-critical environments.

## Technical Specifications

| Feature | Specification |
|---------|---------------|
| **Compliance** | IEEE Std 1003.1-2017 (POSIX.1-2017) |
| **Architecture** | Recursive Descent Parser, AST-based Execution |
| **Memory Model** | Arena-based Stack Allocator for ephemeral data |
| **Process Model** | `vfork()`-optimized command substitution |
| **License** | GPL-2.0-or-later |

## Core Capabilities

### Strict POSIX Compliance
posish adheres strictly to the Shell Command Language specification, ensuring portability and predictable behavior.
- **Full Grammar Support**: Implements the complete POSIX shell grammar, including complex compound commands and redirections.
- **Builtin Utilities**: All required special builtins (e.g., `set`, `export`, `readonly`) and regular builtins are implemented internally.
- **Signal Handling**: robust `trap` implementation with correct signal propagation and handling.
- **Job Control**: Full job control implementation with background process management and terminal signaling.

### High-Performance Execution Engine
The execution engine is optimized for low latency and high throughput.
- **`vfork()` Optimization**: Command substitutions utilize `vfork()` where safe to eliminate page table copying overhead, achieving ~6000x faster execution for trivial subshells compared to standard `fork()`.
- **Zero-Copy Builtins**: Internal commands execute within the shell process, avoiding context switches and memory duplication.
- **Fast-Path Parsing**: Optimized lexer/parser paths for common trivial commands (e.g., assignments, no-ops) reduce execution overhead.
- **Syscall Reduction**: Aggressive buffering and signal initialization strategies minimize kernel boundary crossings.

## OS Support

posish has been succesfully ported and tested to the following operating systems:
- Linux
- QNX
- FreeBSD
- NetBSD

More ports are in progress.

## Installation

### Dependencies
#### Mandatory
- C99 compliant compiler (GCC/Clang)
- POSIX-compliant C library (glibc, musl)
- Meson and Ninja

#### Optional
- **Testing**: Python 3 and `pytest` (to run the test suite)
- **Packaging**: `debhelper` and `devscripts` (to build Debian packages)

### Build Instructions

```sh
git clone https://github.com/xaonfc/posish.git
cd posish
./build.sh
```

This produces the `posish` binary in the project root.

### Cross-Compilation

posish supports cross-compilation for multiple platforms:

```sh
# Build for different operating systems
./build.sh target=qnx      # QNX Neutrino (requires QNX SDP)

# Build for different architectures (Linux)
./build.sh arch=aarch64    # ARM64 Linux
```

| Option | Build Dir | Requirements |
|--------|-----------|--------------|
| `target=qnx` | `build_qnx/` | QNX SDP, `qnx-x86_64.txt` cross-file |
| `arch=aarch64` | `build_aarch64/` | `gcc-aarch64-linux-gnu` package |

## Documentation

Detailed technical documentation is available in the `docs/` directory:

- [**Architecture**](docs/ARCHITECTURE.md): Internal design, memory management, and execution flow.
- [**Performance**](docs/PERFORMANCE.md): Optimization strategies, benchmarks, and profiling data.
- [**Compliance**](docs/COMPLIANCE.md): Standards adherence statement and feature matrix.
- [**Builtins**](docs/BUILTINS.md): Reference for internal command implementations.

## Usage

posish operates as a standard command interpreter:

```sh
# Interactive mode
./posish

# Script execution
./posish script.sh

# Command string execution
./posish -c "command_string"
```

### Trace Mode
Enable execution tracing for debugging:
```sh
./posish -x script.sh
```

## Contributing

Contributions are welcome. Please adhere to the following guidelines:
1.  **Strict POSIX Compliance**: All features must align with IEEE Std 1003.1-2017.
2.  **Code Style**: Follow the existing C99 style guide.
3.  **Testing**: Ensure all regression tests pass (`./build.sh test`).
4.  **Documentation**: Update relevant documentation in `docs/`.

## License

Copyright © 2025 xaonfc (Mario).
Licensed under the GNU General Public License v2.0 or later.
