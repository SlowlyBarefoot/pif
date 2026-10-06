# Host-side unit tests for PIF modules.
#
#   make -C test          build and run every test
#   make -C test clean    remove build output
#
# Any C99 compiler works, e.g. "make -C test CC=clang" or CC="zig cc".

CC      ?= cc
CFLAGS  ?= -std=c99 -O1 -g -Wall -Wextra -Wno-unused-parameter
ROOT    := ..
INC     := -I. -I$(ROOT)/include
BUILD   := build

TESTS   := $(BUILD)/test_pif_bst $(BUILD)/test_pif_bst_key8 $(BUILD)/test_pif_link $(BUILD)/test_pif_link_nolog

# test_pif_link.c includes pif_link.c itself to reach its static functions.
LINK_SRC := protocol/test_pif_link.c $(ROOT)/source/protocol/pif_link_fragment.c \
            $(ROOT)/source/communication/pif_uart.c \
            $(addprefix $(ROOT)/source/core/, pif.c pif_log.c pif_obj_array.c pif_ring_buffer.c pif_task.c \
                pif_task_manager.c pif_timer.c pif_timer_manager.c)
LINK_DEP := $(ROOT)/source/protocol/pif_link.c $(ROOT)/include/protocol/pif_link.h \
            $(ROOT)/include/protocol/pif_link_fragment.h

.PHONY: all run clean

all: run

run: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

$(BUILD)/test_pif_bst: core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

$(BUILD)/test_pif_bst_key8: core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_BST_KEY=8 $(INC) $^ -o $@

$(BUILD)/test_pif_link: $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(LINK_SRC) -o $@

$(BUILD)/test_pif_link_nolog: $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(LINK_SRC) -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
