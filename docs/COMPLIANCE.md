# POSIX Compliance

**posish** is designed to be a strictly compliant implementation of the **IEEE Std 1003.1-2017 (POSIX.1-2017)** Shell Command Language.

## Standards Adherence

The shell implements the full grammar and semantics defined in the [Shell Command Language](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html) specification.

### Supported Features

| Category | Status | Notes |
|----------|--------|-------|
| **Grammar** | Full | Complete implementation of the shell grammar. |
| **Quoting** | Full | Single, double, backslash, and ANSI-C quoting. |
| **Expansion** | Full | Tilde, parameter, command substitution, arithmetic, pathname. |
| **Redirection** | Full | All operators including `<<-`, `<>`, `>&`, `<&`. |
| **Control Flow** | Full | `if`, `for`, `while`, `until`, `case`, functions. |
| **Builtins** | Full | All standard utilities implemented (see `BUILTINS.md`). |
| **Variables** | Full | Standard environment and shell variables supported. |

## Non-Standard Extensions

To ensure maximum portability and strict compliance, posish explicitly **rejects** common non-POSIX extensions found in `bash` or `ksh`.

### Explicitly Removed / Rejected Features

The following features are often assumed to be standard but are **not POSIX** and are rejected by posish:

1.  **Substring Expansion**: `${var:offset:length}`
    - **Status**: Rejected with `Bad substitution` error.
    - **Reason**: Not in POSIX. Use `cut` or `expr` instead.

2.  **Pattern Substitution**: `${var/pattern/replacement}`
    - **Status**: Rejected with `Bad substitution` error.
    - **Reason**: Not in POSIX. Use `sed` instead.

3.  **Here-Strings**: `<<< "string"`
    - **Status**: Not supported (syntax error).
    - **Reason**: Bash/ksh extension. Use `printf "string" | command` or heredocs.

4.  **Arrays**: `var=(a b c)`
    - **Status**: Not supported.
    - **Reason**: POSIX shell only supports string variables. Use positional parameters (`set -- a b c`) for array-like behavior.

5.  **Double Bracket Test**: `[[ expression ]]`
    - **Status**: Not supported.
    - **Reason**: Bash/ksh extension. Use standard `test` or `[ ... ]`.

## Implementation Notes

- **`echo`**: Implements XSI-compliant behavior (supports escape sequences like `\n`, `\t` by default).
- **`kill`**: Supports signal names with or without `SIG` prefix (e.g., `kill -INT` or `kill -SIGINT`).
- **`trap`**: Correctly handles signal resets on subshell entry as per POSIX.
