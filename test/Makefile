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

NAMES   := test_pif_bst test_pif_bst_key8 test_pif_ring_buffer test_pif_ring_data test_pif_sequence test_pif_timer test_pif_pwm_gen test_pif_collect_signal test_pif_gpio test_pif_gpio_cs test_pif_gpio_cs_off test_pif_link test_pif_link_nolog test_pif_can test_pif_can_stream test_pif_device_reg test_pif_ubx_ddc_stream test_pif_spi_fd_stream test_pif_transport test_pif_transport_min test_pif_pulse test_pif_encoder test_pif_task_manager test_pif_solenoid test_pif_solenoid_cs \
           test_pif_dot_matrix test_pif_fnd test_pif_led test_pif_modbus test_pif_modbus_nolog

# test_pif_link.c includes pif_link.c itself to reach its static functions.
LINK_SRC := protocol/test_pif_link.c $(ROOT)/source/protocol/pif_link_fragment.c \
            $(ROOT)/source/communication/pif_uart.c $(ROOT)/source/communication/pif_transport.c \
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
PWM_GEN_SRC := core/test_pif_pwm_gen.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_pwm_gen.c pif_task.c \
               pif_task_manager.c pif_timer.c pif_timer_manager.c)
PWM_GEN_DEP := $(TIMER_DEP) $(ROOT)/include/core/pif_pwm_gen.h
# test_pif_gpio.c includes pif_gpio.c itself to reach its polling task.
GPIO_SRC  := core/test_pif_gpio.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
# With PIF_COLLECT_SIGNAL the channels pull in pif_log.c and what it needs.
GPIO_CS_SRC := $(ROOT)/source/communication/pif_uart.c $(ROOT)/source/communication/pif_transport.c \
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
                  $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
FND_SRC := display/test_pif_fnd.c $(ROOT)/source/display/pif_fnd.c \
           $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
LED_SRC := display/test_pif_led.c $(ROOT)/source/display/pif_led.c \
           $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
MODBUS_SRC := protocol/test_pif_modbus.c $(ROOT)/source/communication/pif_uart.c $(ROOT)/source/communication/pif_transport.c \
              $(addprefix $(ROOT)/source/protocol/, pif_modbus.c pif_modbus_ascii.c pif_modbus_rtu.c pif_modbus_slave.c \
                  pif_modbus_rtu_master.c pif_modbus_rtu_slave.c pif_modbus_ascii_master.c pif_modbus_ascii_slave.c) \
              $(addprefix $(ROOT)/source/core/, pif.c pif_log.c pif_obj_array.c pif_ring_buffer.c pif_task.c \
                  pif_task_manager.c)
MODBUS_DEP := $(wildcard $(ROOT)/include/protocol/pif_modbus*.h)

# test_pif_can.c includes pif_can.c itself to run its tasks directly.
CAN_SRC := communication/test_pif_can.c \
           $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ring_data.c pif_task.c pif_task_manager.c)
CAN_DEP := $(ROOT)/source/communication/pif_can.c $(ROOT)/include/communication/pif_can.h \
           $(ROOT)/include/core/pif_ring_data.h
# test_pif_can_stream.c includes pif_can.c and pif_can_stream.c to run the tasks directly.
CAN_STREAM_SRC := communication/test_pif_can_stream.c $(ROOT)/source/communication/pif_transport.c \
                  $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ring_buffer.c pif_ring_data.c \
                      pif_task.c pif_task_manager.c)
CAN_STREAM_DEP := $(CAN_DEP) $(ROOT)/source/communication/pif_can_stream.c \
                  $(ROOT)/include/communication/pif_can_stream.h $(ROOT)/include/communication/pif_transport.h
# test_pif_ubx_ddc_stream.c includes pif_device_stream.c and pif_ubx_ddc_stream.c to run the task directly.
UBX_DDC_SRC := gps/test_pif_ubx_ddc_stream.c \
               $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_i2c.c pif_transport.c) \
               $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
STREAM_DEP := $(addprefix $(ROOT)/source/communication/, pif_device_stream.c) \
              $(addprefix $(ROOT)/include/communication/, pif_device_reg.h pif_device_stream.h pif_transport.h)
UBX_DDC_DEP := $(STREAM_DEP) $(ROOT)/source/gps/pif_ubx_ddc_stream.c $(ROOT)/include/gps/pif_ubx_ddc_stream.h \
               $(ROOT)/include/communication/pif_i2c.h
# test_pif_spi_fd_stream.c includes pif_device_stream.c and pif_spi_fd_stream.c to run the task directly.
SPI_FD_SRC := communication/test_pif_spi_fd_stream.c \
              $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_spi.c pif_transport.c) \
              $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
SPI_FD_DEP := $(STREAM_DEP) $(ROOT)/source/communication/pif_spi_fd_stream.c \
              $(addprefix $(ROOT)/include/communication/, pif_spi_fd_stream.h pif_spi.h)
# PifDeviceReg over pif_i2c and pif_spi.
DEVICE_REG_SRC := communication/test_pif_device_reg.c \
              $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_i2c.c pif_spi.c) \
              $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
DEVICE_REG_DEP := $(addprefix $(ROOT)/include/communication/, pif_device_reg.h pif_i2c.h pif_spi.h)
# pif_link and pif_xmodem over PifUart and PifCanStream.
TRANSPORT_SRC := protocol/test_pif_transport.c \
                 $(addprefix $(ROOT)/source/communication/, pif_can.c pif_can_stream.c pif_transport.c pif_uart.c) \
                 $(addprefix $(ROOT)/source/protocol/, pif_link.c pif_xmodem.c) \
                 $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ring_buffer.c pif_ring_data.c pif_task.c \
                     pif_task_manager.c pif_timer.c pif_timer_manager.c)
TRANSPORT_DEP := $(wildcard $(ROOT)/include/communication/*.h) $(ROOT)/include/protocol/pif_link.h \
                 $(ROOT)/include/protocol/pif_xmodem.h

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
run-test_pif_pwm_gen: run-test_pif_timer
run-test_pif_collect_signal: run-test_pif_pwm_gen
run-test_pif_gpio: run-test_pif_collect_signal
run-test_pif_gpio_cs: run-test_pif_gpio
run-test_pif_link: run-test_pif_gpio_cs
run-test_pif_link_nolog: run-test_pif_link
run-test_pif_can: run-test_pif_link_nolog
run-test_pif_can_stream: run-test_pif_can
run-test_pif_device_reg: run-test_pif_can_stream
run-test_pif_ubx_ddc_stream: run-test_pif_device_reg
run-test_pif_spi_fd_stream: run-test_pif_ubx_ddc_stream
run-test_pif_transport: run-test_pif_spi_fd_stream
run-test_pif_transport_min: run-test_pif_transport
run-test_pif_pulse: run-test_pif_transport_min
run-test_pif_encoder: run-test_pif_pulse
run-test_pif_task_manager: run-test_pif_encoder
run-test_pif_solenoid: run-test_pif_task_manager
run-test_pif_solenoid_cs: run-test_pif_solenoid
run-test_pif_dot_matrix: run-test_pif_solenoid_cs
run-test_pif_fnd: run-test_pif_dot_matrix
run-test_pif_led: run-test_pif_fnd
run-test_pif_modbus: run-test_pif_led
run-test_pif_modbus_nolog: run-test_pif_modbus

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

$(BUILD)/test_pif_pwm_gen$(EXE): $(PWM_GEN_SRC) $(PWM_GEN_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(PWM_GEN_SRC) -o $@

$(BUILD)/test_pif_collect_signal$(EXE): $(CS_SRC) $(CS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL $(INC) $(CS_SRC) -o $@

$(BUILD)/test_pif_gpio$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(GPIO_SRC) -o $@

$(BUILD)/test_pif_gpio_cs$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL $(INC) $(GPIO_SRC) $(GPIO_CS_SRC) -o $@

# PIF_COLLECT_SIGNAL_NO_GPIO leaves the gpio out of the collector while the collector itself is built.
$(BUILD)/test_pif_gpio_cs_off$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL -DPIF_COLLECT_SIGNAL_NO_GPIO $(INC) $(GPIO_SRC) $(GPIO_CS_SRC) -o $@

$(BUILD)/test_pif_link$(EXE): $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(LINK_SRC) -o $@

$(BUILD)/test_pif_link_nolog$(EXE): $(LINK_SRC) $(LINK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(LINK_SRC) -o $@

$(BUILD)/test_pif_can$(EXE): $(CAN_SRC) $(CAN_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(CAN_SRC) -o $@

$(BUILD)/test_pif_can_stream$(EXE): $(CAN_STREAM_SRC) $(CAN_STREAM_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(CAN_STREAM_SRC) -o $@

$(BUILD)/test_pif_device_reg$(EXE): $(DEVICE_REG_SRC) $(DEVICE_REG_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(DEVICE_REG_SRC) -o $@

$(BUILD)/test_pif_ubx_ddc_stream$(EXE): $(UBX_DDC_SRC) $(UBX_DDC_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(UBX_DDC_SRC) -o $@

$(BUILD)/test_pif_spi_fd_stream$(EXE): $(SPI_FD_SRC) $(SPI_FD_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(SPI_FD_SRC) -o $@

$(BUILD)/test_pif_transport$(EXE): $(TRANSPORT_SRC) $(TRANSPORT_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(TRANSPORT_SRC) -o $@

# PIF_UART_NO_FLOW_CONTROL and PIF_UART_NO_DIRECTION leave the flow control and the RS-485 direction of the UART out.
$(BUILD)/test_pif_transport_min$(EXE): $(TRANSPORT_SRC) $(TRANSPORT_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_UART_NO_FLOW_CONTROL -DPIF_UART_NO_DIRECTION $(INC) $(TRANSPORT_SRC) -o $@

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

$(BUILD)/test_pif_fnd$(EXE): $(FND_SRC) $(ROOT)/include/display/pif_fnd.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(FND_SRC) -o $@

$(BUILD)/test_pif_led$(EXE): $(LED_SRC) $(ROOT)/include/display/pif_led.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(LED_SRC) -o $@

$(BUILD)/test_pif_modbus$(EXE): $(MODBUS_SRC) $(MODBUS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(MODBUS_SRC) -o $@

$(BUILD)/test_pif_modbus_nolog$(EXE): $(MODBUS_SRC) $(MODBUS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(MODBUS_SRC) -o $@

$(BUILD):
	$(call MKDIR,$(BUILD))

clean:
	$(call RMDIR,$(BUILD))
