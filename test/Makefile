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

NAMES   := test_pif_bst test_pif_bst_key8 test_pif_ring_buffer test_pif_ring_data test_pif_sequence test_pif_timer test_pif_pwm_gen test_pif_collect_signal test_pif_gpio test_pif_gpio_cs test_pif_gpio_cs_off test_pif_link test_pif_link_nolog test_pif_can test_pif_can_stream test_pif_device_reg test_pif_ubx_ddc_stream test_pif_spi_fd_stream test_pif_transport test_pif_transport_min test_pif_pulse test_pif_encoder test_pif_sensor test_pif_sensor_modulated test_pif_noise_filter test_pif_task_manager test_pif_solenoid test_pif_solenoid_cs \
           test_pif_dot_matrix test_pif_fnd test_pif_led test_pif_modbus test_pif_modbus_nolog test_pif_rc_serial \
           test_pif_nvram test_pif_nor_flash test_pif_motor test_pif_duty_motor_speed_enc \
           test_pif_printf test_pif_printf_nofloat test_pif_ring_buffer_nolock test_pif_modbus_slave_min \
           test_pif_task_manager_nort test_pif_motor_min test_pif_nvram_raw test_pif_crc test_pif_pid_control_cs \
           test_pif_crc_table test_pif_keypad test_pif_log test_pif_log_cmd test_pif_encoding

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
# test_pif_sensor.c includes pif_max31855.c itself to run its task directly.
SENSOR_SRC := sensor/test_pif_sensor.c \
              $(addprefix $(ROOT)/source/sensor/, pif_sensor.c pif_sensor_switch.c pif_sensor_threshold.c) \
              $(addprefix $(ROOT)/source/filter/, pif_noise_filter.c pif_noise_filter_bit.c pif_noise_filter_int16.c \
                  pif_noise_filter_int32.c) \
              $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_spi.c) \
              $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ptr_array.c pif_task.c \
                  pif_task_manager.c)
SENSOR_DEP := $(addprefix $(ROOT)/include/sensor/, pif_sensor.h pif_sensor_switch.h pif_sensor_threshold.h pif_max31855.h) \
              $(ROOT)/source/sensor/pif_max31855.c
# test_pif_sensor_modulated.c plays the ADC interrupt between the passes of the main loop.
SENSOR_MODULATED_SRC := sensor/test_pif_sensor_modulated.c \
                        $(addprefix $(ROOT)/source/sensor/, pif_sensor.c pif_sensor_modulated.c pif_sensor_threshold.c) \
                        $(ROOT)/source/filter/pif_noise_filter.c \
                        $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
SENSOR_MODULATED_DEP := $(addprefix $(ROOT)/include/sensor/, pif_sensor_modulated.h pif_sensor_threshold.h)
NOISE_FILTER_SRC := filter/test_pif_noise_filter.c \
                    $(addprefix $(ROOT)/source/filter/, pif_noise_filter.c pif_noise_filter_bit.c pif_noise_filter_int16.c \
                        pif_noise_filter_int32.c) \
                    $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
NOISE_FILTER_DEP := $(ROOT)/source/filter/pif_noise_filter_int_impl.h \
                    $(addprefix $(ROOT)/include/filter/, pif_noise_filter.h pif_noise_filter_bit.h pif_noise_filter_int.h \
                        pif_noise_filter_int16.h pif_noise_filter_int32.h)

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
# Every function a slave can handle.
MODBUS_SLAVE_ALL := -DPIF_USE_MODBUS_SLAVE_COILS -DPIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS -DPIF_USE_MODBUS_SLAVE_INPUT_REGISTERS \
                    -DPIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS -DPIF_USE_MODBUS_SLAVE_READ_WRITE_MULTIPLE

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

# The S.BUS, Spektrum and SUMD receivers over an in-memory wire, with their parsers called directly.
RC_SERIAL_SRC := rc/test_pif_rc_serial.c $(ROOT)/source/communication/pif_uart.c $(ROOT)/source/communication/pif_transport.c \
                 $(addprefix $(ROOT)/source/rc/, pif_rc.c pif_rc_sbus.c pif_rc_spektrum.c pif_rc_sumd.c) \
                 $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_ring_buffer.c pif_task.c pif_task_manager.c)
RC_SERIAL_DEP := $(addprefix $(ROOT)/include/rc/, pif_rc.h pif_rc_sbus.h pif_rc_spektrum.h pif_rc_sumd.h)

# pif_nvram over media in memory, with the timer that carries its transfers.
NVRAM_SRC := storage/test_pif_nvram.c $(ROOT)/source/storage/pif_nvram.c \
               $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_i2c.c) \
               $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c pif_timer.c \
                   pif_timer_manager.c)
NVRAM_DEP := $(ROOT)/include/storage/pif_nvram.h
# pif_nor_flash over an SPI NOR flash in memory.
NOR_FLASH_SRC := storage/test_pif_nor_flash.c $(ROOT)/source/storage/pif_nor_flash.c \
                 $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_spi.c) \
                 $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
NOR_FLASH_DEP := $(ROOT)/include/storage/pif_nor_flash.h storage/nor_flash_emu.h
# littlefs on pif_nor_flash. littlefs is not part of pif, so this one is not in "run": give the directory it is in,
#   make -C test LFS_DIR=path/to/littlefs run-test_pif_littlefs
LITTLEFS_SRC := storage/test_pif_littlefs.c $(addprefix $(ROOT)/source/storage/, pif_littlefs.c pif_nor_flash.c) \
                $(addprefix $(ROOT)/source/communication/, pif_device_reg.c pif_spi.c) \
                $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c) \
                $(if $(LFS_DIR),$(LFS_DIR)/lfs.c $(LFS_DIR)/lfs_util.c)
LITTLEFS_DEP := $(NOR_FLASH_DEP) $(ROOT)/include/storage/pif_littlefs.h

MOTOR_SRC := motor/test_pif_motor.c $(addprefix $(ROOT)/source/motor/, pif_motor.c pif_duty_motor.c pif_step_motor.c) \
             $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c pif_timer.c \
                 pif_timer_manager.c)
MOTOR_DEP := $(addprefix $(ROOT)/include/motor/, pif_motor.h pif_duty_motor.h pif_step_motor.h)
# PifDutyMotorSpeedEnc on a simulated motor, in count and in period control.
DMSE_SRC := motor/test_pif_duty_motor_speed_enc.c \
            $(addprefix $(ROOT)/source/motor/, pif_motor.c pif_duty_motor.c pif_duty_motor_speed_enc.c) \
            $(ROOT)/source/sensor/pif_pulse.c $(ROOT)/source/sensor/pif_sensor.c \
            $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_pid_control.c pif_task.c pif_task_manager.c \
                pif_timer.c pif_timer_manager.c)
DMSE_DEP := $(addprefix $(ROOT)/include/motor/, pif_motor.h pif_duty_motor.h pif_duty_motor_speed_enc.h) \
            $(ROOT)/include/sensor/pif_pulse.h $(ROOT)/include/core/pif_pid_control.h

# pif_PrintFormat() through pif_Printf(), with and without %f.
PRINTF_SRC := core/test_pif_printf.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
LOG_SRC := core/test_pif_log.c $(ROOT)/source/communication/pif_uart.c $(ROOT)/source/communication/pif_transport.c \
           $(addprefix $(ROOT)/source/core/, pif.c pif_log.c pif_obj_array.c pif_ring_buffer.c pif_task.c pif_task_manager.c \
               pif_timer.c pif_timer_manager.c)
CRC_SRC := core/test_pif_crc.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
# test_pif_keypad.c includes pif_keypad.c itself to run the state machine of a key without the scheduler.
KEYPAD_SRC := input/test_pif_keypad.c $(addprefix $(ROOT)/source/core/, pif.c pif_obj_array.c pif_task.c pif_task_manager.c)
KEYPAD_DEP := $(ROOT)/source/input/pif_keypad.c $(ROOT)/include/input/pif_keypad.h

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
run-test_pif_sensor: run-test_pif_encoder
run-test_pif_sensor_modulated: run-test_pif_sensor
run-test_pif_noise_filter: run-test_pif_sensor_modulated
run-test_pif_task_manager: run-test_pif_noise_filter
run-test_pif_solenoid: run-test_pif_task_manager
run-test_pif_solenoid_cs: run-test_pif_solenoid
run-test_pif_dot_matrix: run-test_pif_solenoid_cs
run-test_pif_fnd: run-test_pif_dot_matrix
run-test_pif_led: run-test_pif_fnd
run-test_pif_modbus: run-test_pif_led
run-test_pif_modbus_nolog: run-test_pif_modbus
run-test_pif_rc_serial: run-test_pif_modbus_nolog
run-test_pif_nvram: run-test_pif_rc_serial
run-test_pif_nor_flash: run-test_pif_nvram
run-test_pif_motor: run-test_pif_nor_flash
run-test_pif_duty_motor_speed_enc: run-test_pif_motor
run-test_pif_printf: run-test_pif_duty_motor_speed_enc
run-test_pif_printf_nofloat: run-test_pif_printf
run-test_pif_ring_buffer_nolock: run-test_pif_printf_nofloat
run-test_pif_modbus_slave_min: run-test_pif_ring_buffer_nolock
run-test_pif_task_manager_nort: run-test_pif_modbus_slave_min
run-test_pif_motor_min: run-test_pif_task_manager_nort
run-test_pif_nvram_raw: run-test_pif_motor_min
run-test_pif_crc: run-test_pif_nvram_raw
run-test_pif_pid_control_cs: run-test_pif_crc
run-test_pif_crc_table: run-test_pif_pid_control_cs
run-test_pif_keypad: run-test_pif_crc_table
run-test_pif_log: run-test_pif_keypad
run-test_pif_log_cmd: run-test_pif_log
run-test_pif_encoding: run-test_pif_log_cmd

$(addprefix run-, $(NAMES)): run-%: $(BUILD)/%$(EXE)
	$(call RUN,$<)

$(BUILD)/test_pif_bst$(EXE): core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

$(BUILD)/test_pif_bst_key8$(EXE): core/test_pif_bst.c $(ROOT)/source/core/pif_bst.c | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_BST_KEY=8 $(INC) $^ -o $@

$(BUILD)/test_pif_ring_buffer$(EXE): core/test_pif_ring_buffer.c $(ROOT)/source/core/pif_ring_buffer.c \
                                    $(ROOT)/include/core/pif_ring_buffer.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_USE_RING_BUFFER_LOCK $(INC) $(filter %.c,$^) -o $@

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
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL -DPIF_COLLECT_SIGNAL_SEQUENCE $(INC) $(CS_SRC) -o $@

$(BUILD)/test_pif_gpio$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(GPIO_SRC) -o $@

$(BUILD)/test_pif_gpio_cs$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL -DPIF_COLLECT_SIGNAL_GPIO -DPIF_COLLECT_SIGNAL_UART $(INC) $(GPIO_SRC) \
		$(GPIO_CS_SRC) -o $@

# Without PIF_COLLECT_SIGNAL_GPIO the gpio stays out of the collector while the collector itself is built.
$(BUILD)/test_pif_gpio_cs_off$(EXE): $(GPIO_SRC) $(GPIO_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL $(INC) $(GPIO_SRC) $(GPIO_CS_SRC) -o $@

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

# PIF_USE_UART_FLOW_CONTROL and PIF_USE_UART_DIRECTION put the flow control and the RS-485 direction of the UART in.
$(BUILD)/test_pif_transport$(EXE): $(TRANSPORT_SRC) $(TRANSPORT_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_UART_FLOW_CONTROL -DPIF_USE_UART_DIRECTION $(INC) $(TRANSPORT_SRC) -o $@

$(BUILD)/test_pif_transport_min$(EXE): $(TRANSPORT_SRC) $(TRANSPORT_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(TRANSPORT_SRC) -o $@

$(BUILD)/test_pif_pulse$(EXE): $(PULSE_SRC) $(ROOT)/include/sensor/pif_pulse.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(PULSE_SRC) -o $@

$(BUILD)/test_pif_encoder$(EXE): $(ENCODER_SRC) $(ROOT)/include/sensor/pif_encoder.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(ENCODER_SRC) -o $@

$(BUILD)/test_pif_sensor$(EXE): $(SENSOR_SRC) $(SENSOR_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(SENSOR_SRC) -o $@

$(BUILD)/test_pif_sensor_modulated$(EXE): $(SENSOR_MODULATED_SRC) $(SENSOR_MODULATED_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(SENSOR_MODULATED_SRC) -o $@

$(BUILD)/test_pif_noise_filter$(EXE): $(NOISE_FILTER_SRC) $(NOISE_FILTER_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(NOISE_FILTER_SRC) -o $@

$(BUILD)/test_pif_task_manager$(EXE): $(TASK_SRC) $(TASK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_BLOCK_TIME -DPIF_USE_TASK_REALTIME $(INC) $(TASK_SRC) -o $@

$(BUILD)/test_pif_solenoid$(EXE): $(SOLENOID_SRC) $(ROOT)/include/actuator/pif_solenoid.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(SOLENOID_SRC) -o $@

# The signals pull in pif_log.c and what it needs, as for test_pif_gpio_cs. The largest count
# checks the longest signal names.
$(BUILD)/test_pif_solenoid_cs$(EXE): $(SOLENOID_SRC) $(GPIO_CS_SRC) $(ROOT)/include/actuator/pif_solenoid.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_COLLECT_SIGNAL -DPIF_COLLECT_SIGNAL_SOLENOID -DPIF_COLLECT_SIGNAL_UART -DPIF_SOLENOID_MAX_COUNT=255 \
		$(INC) $(SOLENOID_SRC) $(GPIO_CS_SRC) -o $@

# The channels of a controller on the collect signal without its VCD output, as a backend uses it.
PID_CS_SRC := core/test_pif_pid_control_cs.c               $(addprefix $(ROOT)/source/core/, pif.c pif_collect_signal.c pif_obj_array.c pif_pid_control.c pif_task.c               pif_task_manager.c)
$(BUILD)/test_pif_pid_control_cs$(EXE): $(PID_CS_SRC) $(ROOT)/include/core/pif_pid_control.h 		$(ROOT)/include/core/pif_collect_signal.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_COLLECT_SIGNAL -DPIF_COLLECT_SIGNAL_NO_VCD -DPIF_COLLECT_SIGNAL_PID_CONTROL 		$(INC) $(PID_CS_SRC) -o $@

$(BUILD)/test_pif_dot_matrix$(EXE): $(DOT_MATRIX_SRC) $(ROOT)/include/display/pif_dot_matrix.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(DOT_MATRIX_SRC) -o $@

$(BUILD)/test_pif_fnd$(EXE): $(FND_SRC) $(ROOT)/include/display/pif_fnd.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(FND_SRC) -o $@

$(BUILD)/test_pif_led$(EXE): $(LED_SRC) $(ROOT)/include/display/pif_led.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(LED_SRC) -o $@

$(BUILD)/test_pif_modbus$(EXE): $(MODBUS_SRC) $(MODBUS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) $(MODBUS_SLAVE_ALL) $(INC) $(MODBUS_SRC) -o $@

$(BUILD)/test_pif_modbus_nolog$(EXE): $(MODBUS_SRC) $(MODBUS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(MODBUS_SLAVE_ALL) $(INC) $(MODBUS_SRC) -o $@

$(BUILD)/test_pif_rc_serial$(EXE): $(RC_SERIAL_SRC) $(RC_SERIAL_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(RC_SERIAL_SRC) -o $@

$(BUILD)/test_pif_nvram$(EXE): $(NVRAM_SRC) $(NVRAM_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_NVRAM_RECORD $(INC) $(NVRAM_SRC) -o $@

$(BUILD)/test_pif_nor_flash$(EXE): $(NOR_FLASH_SRC) $(NOR_FLASH_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(NOR_FLASH_SRC) -o $@

$(BUILD)/test_pif_motor$(EXE): $(MOTOR_SRC) $(MOTOR_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_STEP_MOTOR_2PHASE -DPIF_USE_STEP_MOTOR_5PHASE -DPIF_USE_STEP_MOTOR_STEP_DIR \
		$(INC) $(MOTOR_SRC) -o $@

$(BUILD)/test_pif_duty_motor_speed_enc$(EXE): $(DMSE_SRC) $(DMSE_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(DMSE_SRC) -o $@

# A module built with its least features, or with only some of the PIF_USE_ options, against the tests of the
# full one.
$(BUILD)/test_pif_printf$(EXE): $(PRINTF_SRC) $(ROOT)/include/core/pif.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_PRINTF_FLOAT $(INC) $(PRINTF_SRC) -o $@

$(BUILD)/test_pif_printf_nofloat$(EXE): $(PRINTF_SRC) $(ROOT)/include/core/pif.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(PRINTF_SRC) -o $@

$(BUILD)/test_pif_ring_buffer_nolock$(EXE): core/test_pif_ring_buffer.c $(ROOT)/source/core/pif_ring_buffer.c \
                                           $(ROOT)/include/core/pif_ring_buffer.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(filter %.c,$^) -o $@

$(BUILD)/test_pif_modbus_slave_min$(EXE): $(MODBUS_SRC) $(MODBUS_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_MODBUS_SLAVE_INPUT_REGISTERS -DPIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS \
		$(INC) $(MODBUS_SRC) -o $@

$(BUILD)/test_pif_task_manager_nort$(EXE): $(TASK_SRC) $(TASK_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_BLOCK_TIME $(INC) $(TASK_SRC) -o $@

$(BUILD)/test_pif_motor_min$(EXE): $(MOTOR_SRC) $(MOTOR_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_STEP_MOTOR_2PHASE $(INC) $(MOTOR_SRC) -o $@

$(BUILD)/test_pif_nvram_raw$(EXE): $(NVRAM_SRC) $(NVRAM_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(NVRAM_SRC) -o $@

.PHONY: run-test_pif_littlefs
run-test_pif_littlefs: $(BUILD)/test_pif_littlefs$(EXE)
	$(call RUN,$<)

$(BUILD)/test_pif_littlefs$(EXE): $(LITTLEFS_SRC) $(LITTLEFS_DEP) | $(BUILD)
	$(if $(LFS_DIR),,$(error LFS_DIR has to name the directory of littlefs))
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_LITTLEFS -DLFS_NO_DEBUG -DLFS_NO_WARN -DLFS_NO_ERROR $(INC) -I$(LFS_DIR) $(LITTLEFS_SRC) -o $@

$(BUILD):
	$(call MKDIR,$(BUILD))

clean:
	$(call RMDIR,$(BUILD))

$(BUILD)/test_pif_crc$(EXE): $(CRC_SRC) $(ROOT)/include/core/pif.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(CRC_SRC) -o $@

$(BUILD)/test_pif_crc_table$(EXE): $(CRC_SRC) $(ROOT)/include/core/pif.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG -DPIF_USE_CRC8_SMBUS_TABLE -DPIF_USE_CRC8_DVB_S2_TABLE $(INC) $(CRC_SRC) -o $@

$(BUILD)/test_pif_keypad$(EXE): $(KEYPAD_SRC) $(KEYPAD_DEP) | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) $(KEYPAD_SRC) -o $@

$(BUILD)/test_pif_log$(EXE): $(LOG_SRC) $(ROOT)/include/core/pif_log.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(LOG_SRC) -o $@

$(BUILD)/test_pif_log_cmd$(EXE): $(LOG_SRC) $(ROOT)/include/core/pif_log.h | $(BUILD)
	$(CC) $(CFLAGS) -Wno-date-time -DPIF_LOG_COMMAND $(INC) $(LOG_SRC) -o $@

$(BUILD)/test_pif_encoding$(EXE): codec/test_pif_encoding.c $(ROOT)/source/codec/pif_encoding.c $(ROOT)/include/codec/pif_encoding.h | $(BUILD)
	$(CC) $(CFLAGS) -DPIF_NO_LOG $(INC) codec/test_pif_encoding.c $(ROOT)/source/codec/pif_encoding.c -o $@
