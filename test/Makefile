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

NAMES   := test_pif_bst test_pif_bst_key8 test_pif_ring_buffer test_pif_ring_data test_pif_sequence test_pif_timer test_pif_collect_signal test_pif_gpio test_pif_gpio_cs test_pif_link test_pif_link_nolog test_pif_pulse test_pif_encoder test_pif_task_manager test_pif_solenoid test_pif_solenoid_cs \
           test_pif_dot_matrix

# test_pif_link.c includes pif_link.c itself to reach its static functions.
LINK_SRC := protocol/test_pif_link.c $(ROOT)/source/protocol/pif_link_fragment.c \
            $(ROOT)/source/communication/pif_uart.c \
            $(addprefix $(ROOT)/source/core/, pif.c pif_log.c pif_obj_array.c pif_ring_buffer.c pif_task.c \
                pif_task_manager.c pif_timer.c pif_timer_manager.c)
# test_pif_collect_signal.c includes pif_collect_signal.c itself and stands in for pif_log.c. It also checks the
# channels of pif_sequence.c.
CS_SRC  := core/test_pif_collect_signal.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ring_buffer.c pif_sequence.c \
               pif_task.c pif_task_manager.c pif_timer.c pif_timer_manager.c)
CS_DEP  := $(ROOT)/source/core/pif_collect_signal.c $(ROOT)/include/core/pif_collect_signal.h \
            $(ROOT)/source/core/pif_sequence.c $(ROOT)/include/core/pif_sequence.h
TIMER_SRC := core/test_pif_timer.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c \
                 pif_task_manager.c pif_timer.c pif_timer_manager.c)
TIMER_DEP := $(ROOT)/include/core/pif_timer.h $(ROOT)/include/core/pif_timer_manager.h
# test_pif_gpio.c includes pif_gpio.c itself to reach its polling task.
GPIO_SRC  := core/test_pif_gpio.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
# With PIF_COLLECT_SIGNAL the channels pull in pif_log.c and what it needs.
GPIO_CS_SRC := $(ROOT)/source/communication/pif_uart.c \
               $(addprefix $(ROOT)/source/core/, pif_collect_signal.c pif_log.c pif_ring_buffer.c pif_timer.c \
                   pif_timer_manager.c)
GPIO_DEP  := $(ROOT)/source/core/pif_gpio.c $(ROOT)/include/core/pif_gpio.h
PULSE_SRC := sensor/test_pif_pulse.c $(ROOT)/source/sensor/pif_pulse.c \
             $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
ENCODER_SRC := sensor/test_pif_encoder.c $(ROOT)/source/sensor/pif_encoder.c \
               $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)

# The slack rule that a trigger waits for exists only with PIF_USE_BLOCK_TIME.
TASK_SRC := core/test_pif_task_manager.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c \
                pif_task_manager.c)
TASK_DEP := $(ROOT)/include/core/pif_task.h $(ROOT)/include/core/pif_task_manager.h
SOLENOID_SRC := actuator/test_pif_solenoid.c $(ROOT)/source/actuator/pif_solenoid.c \
                $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ring_data.c pif_task.c pif_task_manager.c)
DOT_MATRIX_SRC := display/test_pif_dot_matrix.c $(ROOT)/source/display/pif_dot_matrix.c \
                  $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c \
                      pif_timer.c pif_timer_manager.c)

LINK_DEP :=$(ROOT)/source/protocol/pif_link.c $(ROOT)/include/protocol/pif_link.h \
            $(ROOT)/include/protocol/pif_link_fragment.h

.PHONY: all run clean $(addprefix run-, $(NAMES))

all: run

# One target per test, run in order, so make stops at the first failure.
run: $(addprefix run-, $(NAMES))

run-test_pif_bst_key8: run-test_pif_bst
run-test_pif_ring_buffer: run-test_pif_bst_key8
run-test_pif_ring_data: run-test_pif_ring_buffer
run-test_pif_sequence: run-test_pif_ring_data
run-test_pif_timer: run-test_pif_sequence
run-test_pif_collect_signal: run-test_pif_timer
run-test_pif_gpio: run-test_pif_collect_signal
run-test_pif_gpio_cs: run-test_pif_gpio
run-test_pif_link: run-test_pif_gpio_cs
run-test_pif_link_nolog: run-test_pif_link
run-test_pif_pulse: run-test_pif_link_nolog
run-test_pif_encoder: run-test_pif_pulse
run-test_pif_task_manager: run-test_pif_encoder
run-test_pif_solenoid: run-test_pif_task_manager
run-test_pif_solenoid_cs: run-test_pif_solenoid
run-test_pif_dot_matrix: run-test_pif_solenoid_cs

$(addprefix run-, $(NAMES)): run-%: $(BUILD)/%$(EXE)
	$(call RUN,$<)

$(BUILD)/test_pif_bst$(EXE): core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

$(BUILD)/test_pif_bst_key8$(EXE): core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_BST_KEY=8 $(INC) $^ -o $@

$(BUILD)/test_pif_ring_buffer$(EXE): core/test_pif_ring_buffer.c $(ROOT)/source/core/pif_ring_buffer.c \
                                    $(ROOT)/include/core/pif_ring_buffer.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(filter %.c,$^) -o $@

$(BUILD)/test_pif_ring_data$(EXE): core/test_pif_ring_data.c $(ROOT)/source/core/pif_ring_data.c \
                                  $(ROOT)/include/core/pif_ring_data.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(filter %.c,$^) -o $@

$(BUILD)/test_pif_sequence$(EXE):core/test_pif_sequence.c $(ROOT)/source/core/pif_sequence.c \
                                 $(ROOT)/include/core/pif_sequence.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(filter %.c,$^) -o $@

$(BUILD)/test_pif_timer$(EXE): $(TIMER_SRC) $(TIMER_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(TIMER_SRC) -o $@

$(BUILD)/test_pif_collect_signal$(EXE): $(CS_SRC) $(CS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL $(INC) $(CS_SRC) -o $@

$(BUILD)/test_pif_gpio$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(GPIO_SRC) -o $@

$(BUILD)/test_pif_gpio_cs$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL $(INC) $(GPIO_SRC) $(GPIO_CS_SRC) -o $@

$(BUILD)/test_pif_link$(EXE): $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(LINK_SRC) -o $@

$(BUILD)/test_pif_link_nolog$(EXE): $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(LINK_SRC) -o $@

$(BUILD)/test_pif_pulse$(EXE): $(PULSE_SRC) $(ROOT)/include/sensor/pif_pulse.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(PULSE_SRC) -o $@

$(BUILD)/test_pif_encoder$(EXE): $(ENCODER_SRC) $(ROOT)/include/sensor/pif_encoder.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(ENCODER_SRC) -o $@

$(BUILD)/test_pif_task_manager$(EXE): $(TASK_SRC) $(TASK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_BLOCK_TIME $(INC) $(TASK_SRC) -o $@

$(BUILD)/test_pif_solenoid$(EXE): $(SOLENOID_SRC) $(ROOT)/include/actuator/pif_solenoid.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(SOLENOID_SRC) -o $@

# The signals pull in pif_log.c and what it needs, as for test_pif_gpio_cs. The largest count
# checks the longest signal names.
$(BUILD)/test_pif_solenoid_cs$(EXE): $(SOLENOID_SRC) $(GPIO_CS_SRC) $(ROOT)/include/actuator/pif_solenoid.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL -DPIF_SOLENOID_MAX_COUNT=255 $(INC) $(SOLENOID_SRC) $(GPIO_CS_SRC) -o $@

$(BUILD)/test_pif_dot_matrix$(EXE): $(DOT_MATRIX_SRC) $(ROOT)/include/display/pif_dot_matrix.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(DOT_MATRIX_SRC) -o $@

$(BUILD):
	$(call MKDIR,$(BUILD))

clean:
	$(call RMDIR,$(BUILD))
