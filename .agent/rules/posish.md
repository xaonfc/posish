---
trigger: always_on
---

/*
 * Project: posish
 * Purpose: Rules and contributor guidelines for the posish shell implementation
 * Note: Do not commit or track docs/susv4-2018. That directory contains copyrighted
 *       POSIX material and must remain local only.
 */

/* SPDX-License-Identifier: GPL-2.0-or-later */

# posish: rules and contributor guide

Overview

posish is a strictly POSIX compliant command language interpreter implementing IEEE Std 1003.1-2017 (POSIX.1-2017). It is intended as a compact, fast replacement for /bin/sh on embedded and performance-critical systems. The project focuses on producing a correct, secure, and very small binary.

Project goals and priorities

The project’s priorities are:

- Tiny binary size and minimal resource usage.
- Strict POSIX conformance for behaviors defined by the standard.
- Correctness and safety.
- Performance improvements that are measurable and justified.
- Portability across POSIX.1-2017 compliant systems.

Correctness must not be sacrificed without clear, documented justification.

Tone and scope

This document is the authoritative guideline for code, tests, and builds. You are the sole maintainer. All changes should be small, self-contained, and documented. This file is the basis for local workflows, release processes, and developer practices.

Coding standards

Language and standard  
- Use the C99 standard for all C sources.  
- Prefer simple, portable constructs that minimize binary size.

Naming and style  
- Use snake_case for variables and functions.  
- Use ALL_CAPS for macros and compile time constants.  
- Add concise comments for exported functions describing behavior and constraints.

Formatting  
- Use clang-format or another stable formatter. Format must be run locally and enforced by pre-commit hooks if used.  
- The build system supports either gcc or clang. Meson will detect and use the available compiler unless explicitly overridden.

Documentation  
- Record design decisions and nontrivial trade-offs in docs/ARCHITECTURE.md.  
- For any nontrivial syscall or optimization, include a short rationale comment in code and a matching note in docs.

Memory management

- Avoid dynamic allocation when feasible. Prefer stack allocation for bounded buffers.  
- Use xmalloc() and xrealloc() from src/memalloc.c when heap allocation is required.  
- Always check and handle allocation failures.  
- Avoid unnecessary malloc or free unless there is no simpler alternative.  
- Keep ownership and lifetimes explicit in code.

System calls and performance-sensitive code

General guidance  
- Prefer standard, portable POSIX calls. Optimize only after measurement.  
- Isolate platform-specific or complex syscalls behind abstraction layers.

Recommended efficient primitives  
- Prefer execve() for spawning a new program when explicit argv and envp control is needed.  
- Consider posix_spawn() where its semantics and cost fit the use case.  
- Consider vfork() in fork-plus-exec patterns when profiling demonstrates clear benefit. Avoid vfork unless there is measured improvement and safety checks are in place. Any use of vfork must include:  
  - A code comment explaining the reason and expected semantics.  
  - Benchmarks stored in bench/ demonstrating the benefit on target hardware.  
  - Tests that exercise corner cases and validate safety.

Safety considerations  
- vfork has different semantics than fork and can be unsafe when parent and child share state. Do not use vfork in code that mutates parent-visible memory unless fully validated.  
- Document any syscall that trades safety for performance and include tests.

Avoidance guidance

- Avoid heavy or complex constructs that add size without clear benefit.  
- Avoid large or unnecessary standard library calls.  
- Avoid expensive syscalls unless profiling shows they are acceptable or required.  
- Use phrasing in code comments that follows this pattern: “avoid X unless Y” when a risky or costly pattern is used.

Build system and helper scripts

- Primary build system: Meson + Ninja.  
- Use build.sh as the canonical local helper script.

Suggested build.sh commands    
- ./build.sh build  # to build it
- ./build.sh test  # to test it
- ./build.sh deb # to package it with debian format

Provide build options for sanitizers, debug symbols, and cross compilation. Document these options in docs/build.md.

Testing and quality assurance

Local testing  
- Run the full test suite before committing: ./build.sh test unless it is a documentation change. 
- Include unit tests for parsing, evaluation, job control, I/O, and redirection if we didn't have yet.  
- Add integration tests that reflect real-world shell usage.

Static analysis and sanitizers  
- Run static analyzers such as clang-tidy locally.  
- Use sanitizers (ASAN, UBSAN, MSAN) locally when possible to catch memory and undefined behavior issues.

Regression testing  
- Add a regression test for every fixed bug.  
- Use regression markers in tests for known issues until they are fixed.

Benchmarks and performance tracking

- We are going to use ShellBench to compare it against our competitor, FreeBSD sh.
- Currently, the shell outperforms it on every test
- Occasionally use "~/shellbench/shellbench -s ./build_${OS_NAME}/posish,../chimerautils/build/src.freebsd/sh/sh -t 1 ~/shellbench/output" to test performance. Note: Higher numbers means better performance.

Security and privilege model

- Validate inputs derived from the environment or untrusted sources.  
- Do not execute untrusted data unless explicitly validated.  
- Minimize code executed with elevated privileges. Avoid setuid when possible. If setuid is required, restrict the privileged code path and audit it closely.  
- Log security-sensitive operations where feasible for debugging.

Portability and platform differences

- Target POSIX.1-2017 compliance.  
- Guard platform-specific code with compile-time checks and document differences in docs/platforms.md.  
- Do not add docs/susv4-2018 to version control. They are copyrighted docs from The Open Group.

Code review and commits

- Keep commits small and focused on a single logical change.  
- Write clear commit messages describing rationale and referencing issues when applicable.  
- Maintain a CHANGELOG.md for user-visible changes.  
- As a sole maintainer, apply the same rigor you would expect in external reviews.

Licensing and file headers

- Every source file must begin with the exact line:  
  /* SPDX-License-Identifier: GPL-2.0-or-later */  
- Add a short copyright notice when appropriate.  
- Include the full LICENSE file at the project root. (we already did that)



Miscellaneous guidelines

- Keep third-party dependencies minimal and well-audited.  
- Keep docs clear and up to date.  
- Maintain architecture, build, and developer guides under docs/.

