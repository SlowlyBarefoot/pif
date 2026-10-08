# Host-side unit tests for PIF modules.
#
#   make -C test          build and run every test
#   make -C test clean    remove build output
#
# The compiler defaults to "zig cc" on every platform. Any C99 compiler works,
# e.g. "make -C test CC=gcc" or CC=clang.
# On Windows it runs under cmd.exe (mingw32-make) as well as under a POSIX
# shell (Git Bash, MSYS2).

ifeq ($(OS),Windows_NT)
  EXE := .exe
endif

# make predefines CC as cc, so "?=" would never apply.
ifeq ($(origin CC),default)
  CC := zig cc
endif

CFLAGS  ?= -std=c99 -O1 -g -Wall -Wextra -Wno-unused-parameter
ROOT    := ..
INC     := -I. -I$(ROOT)/include
BUILD   := build

# cmd.exe keeps the quotes that a POSIX shell strips.
ifeq ($(shell echo "q"),"q")
  MKDIR = if not exist $(subst /,\,$1) mkdir $(subst /,\,$1)
  RMDIR = if exist $(subst /,\,$1) rmdir /s /q $(subst /,\,$1)
  RUN   = $(subst /,\,$1)
else
  MKDIR = mkdir -p $1
  RMDIR = rm -rf $1
  RUN   = $1
endif

NAMES   := test_pif_bst test_pif_bst_key8 test_pif_sequence test_pif_link test_pif_link_nolog

# test_pif_link.c includes pif_link.c itself to reach its static functions.
LINK_SRC := protocol/test_pif_link.c $(ROOT)/source/protocol/pif_link_fragment.c \
            $(ROOT)/source/communication/pif_uart.c \
            $(addprefix $(ROOT)/source/core/, pif.c pif_log.c pif_obj_array.c pif_ring_buffer.c pif_task.c \
                pif_task_manager.c pif_timer.c pif_timer_manager.c)
LINK_DEP := $(ROOT)/source/protocol/pif_link.c $(ROOT)/include/protocol/pif_link.h \
            $(ROOT)/include/protocol/pif_link_fragment.h

.PHONY: all run clean $(addprefix run-, $(NAMES))

all: run

# One target per test, run in order, so make stops at the first failure.
run: $(addprefix run-, $(NAMES))

run-test_pif_bst_key8: run-test_pif_bst
run-test_pif_sequence: run-test_pif_bst_key8
run-test_pif_link: run-test_pif_sequence
run-test_pif_link_nolog: run-test_pif_link

$(addprefix run-, $(NAMES)): run-%: $(BUILD)/%$(EXE)
	$(call RUN,$<)

$(BUILD)/test_pif_bst$(EXE): core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

$(BUILD)/test_pif_bst_key8$(EXE): core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_BST_KEY=8 $(INC) $^ -o $@

$(BUILD)/test_pif_sequence$(EXE): core/test_pif_sequence.c $(ROOT)/source/core/pif_sequence.c \
                                 $(ROOT)/include/core/pif_sequence.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(filter %.c,$^) -o $@

$(BUILD)/test_pif_link$(EXE): $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(LINK_SRC) -o $@

$(BUILD)/test_pif_link_nolog$(EXE): $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(LINK_SRC) -o $@

$(BUILD):
	$(call MKDIR,$(BUILD))

clean:
	$(call RMDIR,$(BUILD))
