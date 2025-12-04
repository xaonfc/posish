project('posish', 'c',
  version : '0.0.1',
  default_options : ['warning_level=2', 'c_std=gnu99', 'buildtype=release'])

# Source files
sources = files(
  'src/main.c',
  'src/executor.c',
  'src/lexer.c',
  'src/parser.c',
  'src/variables.c',
  'src/jobs.c',
  'src/signals.c',
  'src/input.c',
  'src/output.c',
  'src/error.c',
  'src/memalloc.c',
  'src/functions.c',
  'src/alias.c',
  'src/shell_options.c',
  'src/buf_output.c',
  'src/line_editor.c',
  'src/ast.c',
  'src/redirection.c',
  'src/builtin-cmds/cd.c',
  'src/builtin-cmds/exit.c',
  'src/builtin-cmds/export.c',
  'src/builtin-cmds/unset.c',
  'src/builtin-cmds/dot.c',
  'src/builtin-cmds/echo.c',
  'src/builtin-cmds/printf.c',
  'src/builtin-cmds/eval.c',
  'src/builtin-cmds/exec.c',
  'src/builtin-cmds/read.c',
  'src/builtin-cmds/set.c',
  'src/builtin-cmds/alias_cmd.c',
  'src/builtin-cmds/unalias.c',
  'src/builtin-cmds/jobs_cmd.c',
  'src/builtin-cmds/bg.c',
  'src/builtin-cmds/fg.c',
  'src/builtin-cmds/test.c',
  'src/builtin-cmds/kill.c',
  'src/builtin-cmds/break.c',
  'src/builtin-cmds/continue.c',
  'src/builtin-cmds/return.c',
  'src/builtin-cmds/shift.c',
  'src/builtin-cmds/wait.c',
  'src/builtin-cmds/pwd.c',
  'src/builtin-cmds/type.c',
  'src/builtin-cmds/trap.c',
  'src/builtin-cmds/times.c',
  'src/builtin-cmds/umask.c',
  'src/builtin-cmds/command.c',
  'src/builtin-cmds/readonly.c',
  'src/builtin-cmds/getopts.c',
  'src/builtin-cmds/local.c',
  'src/builtin-cmds/true_false.c',
  'src/builtin-cmds/dispatcher.c'
)

inc = include_directories('include')

# Compiler flags
cc = meson.get_compiler('c')
add_project_arguments('-D_XOPEN_SOURCE=700', '-D_DEFAULT_SOURCE', language : 'c')

# QNX Support
deps = []
if host_machine.system() == 'qnx'
  add_project_arguments('-D_QNX_SOURCE', language : 'c')
  # QNX often needs libsocket for networking (if we use it)
  socket_dep = cc.find_library('socket', required : false)
  if socket_dep.found()
    deps += socket_dep
  endif
endif

# Executable
executable('posish',
  sources,
  include_directories : inc,
  dependencies : deps,
  install : true)

# Man page
install_man('src/posish.1')
