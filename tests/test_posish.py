import subprocess
import os
import pytest
import shutil
import tempfile

import platform

# Determine binary path based on OS (Meson build layout)
SYSTEM = platform.system()
POSISH_PATH = os.path.abspath(f"./build_{SYSTEM}/posish")

# Fallback to root if not found (e.g. manual copy)
if not os.path.exists(POSISH_PATH) and os.path.exists("./posish"):
    POSISH_PATH = os.path.abspath("./posish")

def run_posish(command, input_data=None):
    """Runs a command string in posish and returns (stdout, stderr, return_code)."""
    process = subprocess.run(
        [POSISH_PATH, "-c", command],
        input=input_data,
        capture_output=True,
        text=True,
        timeout=2
    )
    return process.stdout.strip(), process.stderr, process.returncode

def run_posish_script(script_content):
    """Runs a script content in posish and returns stdout."""
    process = subprocess.run(
        [POSISH_PATH],
        input=script_content,
        capture_output=True,
        text=True,
        timeout=2
    )
    return process.stdout.strip()

# ============================================================================
# CATEGORY: Basic Commands
# ============================================================================

def test_echo_basic():
    assert run_posish("echo hello")[0] == "hello"
    assert run_posish("echo hello world")[0] == "hello world"
    assert run_posish("echo")[0] == ""
    
def test_echo_options():
    assert run_posish("echo -n hello")[0] == "hello"
    assert run_posish("echo -n a b c")[0] == "a b c"

def test_colon_command():
    assert run_posish(":")[0] == ""
    assert run_posish(": ; echo ok")[0] == "ok"

def test_true_false():
    assert run_posish("true")[2] == 0
    assert run_posish("false")[2] == 1
    assert run_posish("false ; echo $?")[0] == "1"
    assert run_posish("true ; echo $?")[0] == "0"

# ============================================================================
# CATEGORY: Variables
# ============================================================================

def test_variables_basic():
    assert run_posish("VAR=test; echo $VAR")[0] == "test"
    assert run_posish("VAR='hello world'; echo \"$VAR\"")[0] == "hello world"
    assert run_posish("VAR=test; unset VAR; echo \"$VAR\"")[0] == ""

def test_variables_numbers():
    assert run_posish("NUM=42; echo $NUM")[0] == "42"
    assert run_posish("A=1 B=2 C=3; echo $A$B$C")[0] == "123"

def test_variable_assignment_inline():
    assert run_posish("VAR=value echo")[0] == ""
    assert run_posish("VAR=value; echo $VAR")[0] == "value"

def test_readonly_variables():
    stdout, _, code = run_posish("readonly VAR=test; VAR=new 2\u003e\u00261")
    assert code != 0  # Should fail

def test_export_variables():
   assert run_posish("export VAR=exported; echo $VAR")[0] == "exported"

#  ============================================================================
# CATEGORY: Parameter Expansion
# ============================================================================

def test_parameter_expansion_basic():
    assert run_posish("VAR=hello; echo ${VAR}")[0] == "hello"
    assert run_posish("VAR=; echo \"${VAR:-default}\"")[0] == "default"
    assert run_posish("VAR=value; echo \"${VAR:-default}\"")[0] == "value"

def test_parameter_expansion_colon_minus():
    assert run_posish("unset VAR; echo \"${VAR:-default}\"")[0] == "default"
    assert run_posish("VAR=; echo \"${VAR:-default}\"")[0] == "default"
    assert run_posish("VAR=set; echo \"${VAR:-default}\"")[0] == "set"

def test_parameter_expansion_colon_plus():
    assert run_posish("unset VAR; echo \"${VAR:+alt}\"")[0] == ""
    assert run_posish("VAR=; echo \"${VAR:+alt}\"")[0] == ""
    assert run_posish("VAR=set; echo \"${VAR:+alt}\"")[0] == "alt"

def test_parameter_expansion_colon_equal():
    assert run_posish("unset VAR; echo \"${VAR:=default}\"; echo $VAR")[0] == "default\ndefault"

def test_parameter_expansion_length():
    assert run_posish("VAR=hello; echo ${#VAR}")[0] == "5"
    assert run_posish("VAR=; echo ${#VAR}")[0] == "0"

def test_parameter_expansion_pattern_removal():
    assert run_posish("VAR=hello.txt; echo ${VAR%.txt}")[0] == "hello"
    assert run_posish("VAR=/path/to/file; echo ${VAR##*/}")[0] == "file"
    assert run_posish("VAR=prefix_name; echo ${VAR#prefix_}")[0] == "name"

# ============================================================================
# CATEGORY: Special Parameters
# ============================================================================

def test_special_params_dollar_question():
    assert run_posish("true; echo $?")[0] == "0"
    assert run_posish("false; echo $?")[0] == "1"
    assert run_posish("sh -c 'exit 42'; echo $?")[0] == "42"

def test_special_params_dollar_hash():
    assert run_posish("set -- a b c; echo $#")[0] == "3"
    assert run_posish("set --; echo $#")[0] == "0"

def test_special_params_dollar_at():
    assert run_posish("set -- a b c; echo \"$@\"")[0] == "a b c"
    assert run_posish("set -- \"a b\" c; echo $#")[0] == "2"

def test_special_params_dollar_star():
    assert run_posish("set -- a b c; echo $*")[0] == "a b c"

def test_special_params_dollar_dollar():
    stdout, _, _ = run_posish("echo $$")
    assert stdout.isdigit()  # Should be a PID

# ============================================================================
# CATEGORY: Control Flow
# ============================================================================

def test_if_then_else():
    assert run_posish("if true; then echo yes; else echo no; fi")[0] == "yes"
    assert run_posish("if false; then echo yes; else echo no; fi")[0] == "no"
    
def test_if_elif():
    cmd = "VAR=2; if [ \"$VAR\" = 1 ]; then echo one; elif [ \"$VAR\" = 2 ]; then echo two; else echo other; fi"
    assert run_posish(cmd)[0] == "two"

def test_while_loop():
    script = """
    i=0
    while [ $i -lt 3 ]; do
        echo $i
        i=$((i+1))
    done
    """
    assert run_posish(script)[0] == "0\n1\n2"

def test_for_loop():
    assert run_posish("for i in a b c; do echo $i; done")[0] == "a\nb\nc"
    assert run_posish("for i in 1 2 3; do echo num$i; done")[0] == "num1\nnum2\nnum3"

def test_case_statement():
    cmd = """
    case hello in
        hello) echo match ;;
        *) echo nomatch ;;
    esac
    """
    assert run_posish(cmd)[0] == "match"
    
    cmd2 = """
    case test in
        hello) echo match ;;
        *) echo nomatch ;;
    esac
    """
    assert run_posish(cmd2)[0] == "nomatch"

def test_break_continue():
    script = """
    for i in 1 2 3 4 5; do
        if [ $i = 3 ]; then continue; fi
        if [ $i = 5 ]; then break; fi
        echo $i
    done
    """
    assert run_posish(script)[0] == "1\n2\n4"

# ============================================================================
# CATEGORY: Functions
# ============================================================================

def test_functions_basic():
    assert run_posish("foo() { echo bar; }; foo")[0] == "bar"
    assert run_posish("foo() { echo $1 $2; }; foo a b")[0] == "a b"

def test_functions_positional_params():
    script = """
    func() {
        echo "args: $#"
        echo "first: $1"
        echo "all: $@"
    }
    func one two three
    """
    expected = "args: 3\nfirst: one\nall: one two three"
    assert run_posish(script)[0] == expected

def test_functions_local_variables():
    script = """
    VAR=global
    foo() {
        local VAR=local
        echo $VAR
    }
    foo
    echo $VAR
    """
    assert run_posish(script)[0] == "local\nglobal"

def test_functions_return():
    script = """
    func() {
        return 42
    }
    func
    echo $?
    """
    assert run_posish(script)[0] == "42"

def test_functions_nested():
    script = """
    outer() {
        inner() {
            echo nested
        }
        inner
    }
    outer
    """
    assert run_posish(script)[0] == "nested"

# ============================================================================
# CATEGORY: Redirections
# ============================================================================

def test_redirection_output(tmp_path):
    output_file = tmp_path / "output.txt"
    cmd = f"echo hello \u003e {output_file}; cat {output_file}"
    assert run_posish(cmd)[0] == "hello"

def test_redirection_append(tmp_path):
    append_file = tmp_path / "append.txt"
    cmd = f"echo line1 \u003e {append_file}; echo line2 \u003e\u003e {append_file}; cat {append_file}"
    assert run_posish(cmd)[0] == "line1\nline2"

def test_redirection_input(tmp_path):
    input_file = tmp_path / "input.txt"
    input_file.write_text("file content")
    cmd = f"cat \u003c {input_file}"
    assert run_posish(cmd)[0] == "file content"

def test_redirection_stderr():
    # Redirect stderr to stdout
    cmd = "sh -c 'echo error \u003e\u00262' 2\u003e\u00261"
    stdout, _, _ = run_posish(cmd)
    assert "error" in stdout

def test_here_document():
    script = """
    cat \u003c\u003cEOF
line1
line2
EOF
    """
    assert run_posish(script)[0] == "line1\nline2"

# ============================================================================
# CATEGORY: Pipes and Command Substitution
# ============================================================================

def test_pipe_basic():
    assert run_posish("echo hello | cat")[0] == "hello"
    assert run_posish("echo a b c | wc -w")[0].strip() == "3"

def test_pipe_chain():
    assert run_posish("echo hello | cat | cat")[0] == "hello"

def test_command_substitution_backtick():
    assert run_posish("echo `echo nested`")[0] == "nested"
    assert run_posish("VAR=`echo value`; echo $VAR")[0] == "value"

def test_command_substitution_dollar_paren():
    assert run_posish("echo $(echo nested)")[0] == "nested"
    assert run_posish("VAR=$(echo value); echo $VAR")[0] == "value"

def test_command_substitution_nested():
    assert run_posish("echo $(echo $(echo deep))")[0] == "deep"

# ============================================================================
# CATEGORY: Logical Operators
# ============================================================================

def test_and_operator():
    assert run_posish("true \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("false \u0026\u0026 echo no")[0] == ""
    assert run_posish("true \u0026\u0026 true \u0026\u0026 echo yes")[0] == "yes"

def test_or_operator():
    assert run_posish("false || echo yes")[0] == "yes"
    assert run_posish("true || echo no")[0] == ""
    assert run_posish("false || false || echo yes")[0] == "yes"

def test_and_or_combined():
    assert run_posish("true \u0026\u0026 echo a || echo b")[0] == "a"
    assert run_posish("false \u0026\u0026 echo a || echo b")[0] == "b"

# ============================================================================
# CATEGORY: Arithmetic Expansion
# ============================================================================

def test_arithmetic_basic():
    assert run_posish("echo $((1+1))")[0] == "2"
    assert run_posish("echo $((5*3))")[0] == "15"
    assert run_posish("echo $((10-3))")[0] == "7"
    assert run_posish("echo $((20/4))")[0] == "5"

def test_arithmetic_variables():
    assert run_posish("A=5; B=3; echo $((A+B))")[0] == "8"
    assert run_posish("N=10; echo $((N*2))")[0] == "20"

def test_arithmetic_precedence():
    assert run_posish("echo $((2+3*4))")[0] == "14"
    assert run_posish("echo $((( 2+3)*4))")[0] == "20"

# ============================================================================
# CATEGORY: Test Builtin / Conditionals
# ============================================================================

def test_test_string_equal():
    assert run_posish("[ a = a ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ a = b ] || echo no")[0] == "no"

def test_test_string_not_equal():
    assert run_posish("[ a != b ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ a != a ] || echo no")[0] == "no"

def test_test_string_empty():
    assert run_posish("[ -z \"\" ] \u0026\u0026 echo empty")[0] == "empty"
    assert run_posish("[ -n \"text\" ] \u0026\u0026 echo nonempty")[0] == "nonempty"

def test_test_numeric_comparisons():
    assert run_posish("[ 5 -eq 5 ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ 5 -ne 3 ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ 5 -gt 3 ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ 3 -lt 5 ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ 5 -ge 5 ] \u0026\u0026 echo yes")[0] == "yes"
    assert run_posish("[ 5 -le 5 ] \u0026\u0026 echo yes")[0] == "yes"

def test_test_file_existence(tmp_path):
    test_file = tmp_path / "test.txt"
    test_file.write_text("content")
    assert run_posish(f"[ -f {test_file} ] \u0026\u0026 echo exists")[0] == "exists"
    assert run_posish(f"[ -d {tmp_path} ] \u0026\u0026 echo isdir")[0] == "isdir"

def test_test_empty_string_quoted():
    # Regression test for empty quoted variable
    assert run_posish('s=""; [ "$s" = "$s" ] \u0026\u0026 echo PASS')[0] == "PASS"

# ============================================================================
# CATEGORY: Builtins
# ============================================================================

def test_cd_pwd(tmp_path):
    cmd = f"cd {tmp_path}; pwd"
    assert run_posish(cmd)[0] == str(tmp_path)

def test_alias():
    script = """
    alias ll='echo listed'
    ll
    """
    assert run_posish_script(script) == "listed"

def test_unalias():
    script = """
    alias foo=echo
    unalias foo
    """
    # After unalias, foo should not be recognized (would fail if called)
    assert run_posish_script(script) == ""

def test_shift():
    script = """
    set -- a b c
    echo $1
    shift
    echo $1
    shift 2
    echo $#
    """
    assert run_posish(script)[0] == "a\nb\n0"

def test_set_options():
    # Test -x (trace mode) - should print commands to stderr
    _, stderr, _ = run_posish("set -x; echo test")
    assert "+ echo test" in stderr or "echo test" in stderr

def test_set_positional():
    assert run_posish("set -- x y z; echo $1 $2 $3")[0] == "x y z"
    assert run_posish("set -- a; echo $#")[0] == "1"

def test_getopts():
    script = """
    while getopts 'ab:' opt; do
        case $opt in
            a) echo 'got a' ;;
            b) echo "got b: $OPTARG" ;;
        esac
    done
    """
    # This would need actual args passed to the script
    # For now just verify it doesn't crash
    assert run_posish(script)[2] == 0

def test_printf():
    assert run_posish("printf 'hello'")[0] == "hello"
    assert run_posish("printf '%s\\n' test")[0] == "test"
    assert run_posish("printf '%d\\n' 42")[0] == "42"

def test_read():
    stdout, _, _ = run_posish("read VAR; echo $VAR", input_data="input\n")
    assert stdout == "input"

# ============================================================================
# CATEGORY: Quoting and Escaping
# ============================================================================

def test_single_quotes():
    assert run_posish("echo 'hello world'")[0] == "hello world"
    assert run_posish("VAR=test; echo '$VAR'")[0] == "$VAR"  # No expansion in single quotes

def test_double_quotes():
    assert run_posish("echo \"hello world\"")[0] == "hello world"
    assert run_posish("VAR=test; echo \"$VAR\"")[0] == "test"  # Expansion in double quotes

def test_backslash_escape():
    assert run_posish("echo \\$VAR")[0] == "$VAR"
    assert run_posish("echo hello\\ world")[0] == "hello world"

def test_mixed_quoting():
    assert run_posish("echo \"It's a test\"")[0] == "It's a test"
    assert run_posish("echo 'It\"s a test'")[0] == 'It"s a test'

# ============================================================================
# CATEGORY: Globbing
# ============================================================================

def test_glob_asterisk(tmp_path):
    (tmp_path / "file1.txt").write_text("")
    (tmp_path / "file2.txt").write_text("")
    (tmp_path / "other.log").write_text("")
    
    # Change to tmp_path and test globbing
    cmd = f"cd {tmp_path}; echo *.txt"
    output = run_posish(cmd)[0]
    assert "file1.txt" in output and "file2.txt" in output
    assert "other.log" not in output

def test_glob_question(tmp_path):
    (tmp_path / "a").write_text("")
    (tmp_path / "b").write_text("")
    (tmp_path / "ab").write_text("")
    
    cmd = f"cd {tmp_path}; echo ?"
    output = run_posish(cmd)[0]
    assert "a" in output and "b" in output
    assert "ab" not in output

# ============================================================================
# CATEGORY: Subshells and Grouping
# ============================================================================

def test_subshell():
    assert run_posish("(echo hello)")[0] == "hello"
    assert run_posish("VAR=outer; (VAR=inner; echo $VAR); echo $VAR")[0] == "inner\nouter"

def test_brace_group():
    assert run_posish("{ echo hello; }")[0] == "hello"
    assert run_posish("VAR=outer; { VAR=inner; echo $VAR; }; echo $VAR")[0] == "inner\ninner"

# ============================================================================
# CATEGORY: Exit Status
# ============================================================================

def test_exit_status():
    assert run_posish("exit 0")[2] == 0
    assert run_posish("exit 1")[2] == 1
    assert run_posish("exit 42")[2] == 42

def test_exit_in_function():
    # Exit in function should exit the shell
    script = """
    func() {
        exit 99
    }
    func
    echo "should not print"
    """
    stdout, _, code = run_posish(script)
    assert code == 99
    assert "should not print" not in stdout

# ============================================================================
# CATEGORY: Edge Cases and Regressions
# ============================================================================

def test_empty_command():
    assert run_posish("")[2] == 0
    assert run_posish("   ")[2] == 0
    assert run_posish("# comment")[2] == 0

def test_comment_handling():
    assert run_posish("echo test # comment")[0] == "test"
    assert run_posish("# full line comment\necho ok")[0] == "ok"

def test_multiple_commands():
    assert run_posish("echo a; echo b; echo c")[0] == "a\nb\nc"

def test_long_command_line():
    # Test with reasonably long command
    cmd = "echo " + " ".join([f"arg{i}" for i in range(100)])
    stdout, _, code = run_posish(cmd)
    assert code == 0
    assert "arg0" in stdout and "arg99" in stdout

def test_special_chars_in_strings():
    assert run_posish("echo 'test!@#$%^&*()'")[0] == "test!@#$%^&*()"
    assert run_posish('echo "test<>\u003e|"')[0] == "test<>\u003e|"

# ============================================================================
# CATEGORY: Regression Tests (Recent Fixes)
# ============================================================================

def test_regression_buffered_output_duplication():
    # Test for the bug where buffered output was duplicated in child processes
    # causing arithmetic expansion pipelines to output double or fail.
    # This specifically targets the fix involving buf_out_reset_all() on fork.
    
    # Arithmetic expansion uses pipelines internally in some paths? 
    # Or purely echo | cat scenarios.
    
    # Case 1: Arithmetic pipeline
    assert run_posish("echo $((1+1)) | cat")[0] == "2"
    
    # Case 2: Simple pipeline with potentially buffered output
    assert run_posish("echo 153 | cut -b 1-3")[0] == "153"
    
    # Case 3: Multiple echos
    assert run_posish("echo a; echo b | cat")[0] == "a\nb"

def test_regression_positional_set_optimization():
    # Verify set -- optimization (reusing buffers) doesn't corrupt data
    script = """
    set -- start
    echo $1
    set -- changed
    echo $1
    set -- longer_string
    echo $1
    set -- s
    echo $1
    """
    expected = "start\nchanged\nlonger_string\ns"
    assert run_posish(script)[0] == expected
