CC ?= cc
CFLAGS = -Wall -Wextra -g -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Iinclude -Werror -O3 -march=native -flto
LDFLAGS = -flto
DEBFLAGS ?= -us -uc
VERSION ?= 0.0.1
export DEBEMAIL=xaonfc@linuxmail.org
export DEBFULLNAME=xaonfc (Mario)

SRC_DIR = src
OBJ_DIR = obj
BIN = posish

SRCBUILTIN_OBJS = $(OBJ_DIR)/builtin-cmds/cd.o \
               $(OBJ_DIR)/builtin-cmds/exit.o \
               $(OBJ_DIR)/builtin-cmds/export.o \
               $(OBJ_DIR)/builtin-cmds/unset.o \
               $(OBJ_DIR)/builtin-cmds/dot.o \
               $(OBJ_DIR)/builtin-cmds/echo.o \
               $(OBJ_DIR)/builtin-cmds/printf.o \
               $(OBJ_DIR)/builtin-cmds/eval.o \
               $(OBJ_DIR)/builtin-cmds/exec.o \
               $(OBJ_DIR)/builtin-cmds/read.o \
               $(OBJ_DIR)/builtin-cmds/set.o \
               $(OBJ_DIR)/builtin-cmds/alias.o \
               $(OBJ_DIR)/builtin-cmds/unalias.o \
               $(OBJ_DIR)/builtin-cmds/jobs.o \
               $(OBJ_DIR)/builtin-cmds/bg.o \
               $(OBJ_DIR)/builtin-cmds/fg.o \
               $(OBJ_DIR)/builtin-cmds/test.o \
               $(OBJ_DIR)/builtin-cmds/kill.o \
               $(OBJ_DIR)/builtin-cmds/break.o \
               $(OBJ_DIR)/builtin-cmds/continue.o \
               $(OBJ_DIR)/builtin-cmds/return.o \
               $(OBJ_DIR)/builtin-cmds/shift.o \
               $(OBJ_DIR)/builtin-cmds/wait.o \
               $(OBJ_DIR)/builtin-cmds/pwd.o \
               $(OBJ_DIR)/builtin-cmds/type.o \
               $(OBJ_DIR)/builtin-cmds/trap.o \
               $(OBJ_DIR)/builtin-cmds/times.o \
               $(OBJ_DIR)/builtin-cmds/umask.o \
               $(OBJ_DIR)/builtin-cmds/command.o \
               $(OBJ_DIR)/builtin-cmds/readonly.o \
               $(OBJ_DIR)/builtin-cmds/getopts.o \
               $(OBJ_DIR)/builtin-cmds/local.o \
               $(OBJ_DIR)/builtin-cmds/true_false.o \
               $(OBJ_DIR)/builtin-cmds/dispatcher.o

SRCS = $(wildcard src/*c) $(wildcard src/builtin-cmds/*.c)
OBJS = $(patsubst src/%.c, obj/%.o, $(wildcard src/*.c)) \
       $(patsubst src/builtin-cmds/%.c, obj/builtin-cmds/%.o, $(wildcard src/builtin-cmds/*.c))

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/builtin-cmds/%.o: $(SRC_DIR)/builtin-cmds/%.c | $(OBJ_DIR)/builtin-cmds
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/builtin-cmds:
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN)

.PHONY: all clean distclean deb tests help

deb:
	rm -f debian/changelog.dch
	dch -v $(VERSION)-1 --distribution unstable "Build version $(VERSION)"
	dpkg-buildpackage $(DEBFLAGS)
	mv ../posish_*.deb .
	mv ../posish_*.changes .
	mv ../posish_*.buildinfo .
	mv ../posish_*.dsc .
	mv ../posish_*.tar.xz .
	mv ../posish-dbgsym_*.ddeb .

tests: $(BIN)
	pytest tests/

distclean: clean
	rm -f posish_*.deb posish_*.changes posish_*.buildinfo posish_*.dsc posish_*.tar.xz posish-dbgsym_*.ddeb
	rm -rf debian/.debhelper debian/posish debian/files debian/*.substvars debian/debhelper-build-stamp debian/changelog.dch

help:
	@echo "Available targets:"
	@echo "  all       : Build the posish binary (default)"
	@echo "  clean     : Remove build artifacts"
	@echo "  distclean : Remove build artifacts and packaging files"
	@echo "  deb       : Build Debian package (requires devscripts)"
	@echo "              Usage: make deb VERSION=1.0.0"
	@echo "  tests     : Run the Python test suite (requires pytest)"
	@echo "  help      : Show this help message"
