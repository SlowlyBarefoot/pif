// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_CONF_H
#define PIF_CONF_H


// -------- pif Configuration --------------------

//#define PIF_DEBUG

//#define PIF_STATIC_INLINE			static inline

//#define PIF_WEAK							__attribute__ ((weak))


// -------- pifAdc -------------------------------

//#define PIF_ADC_MAX_CHANNELS				8


// -------- pifAds1x1x ---------------------------

// Conversions no longer than this (us) are waited out with the CPU held instead of through the timer.
//#define PIF_ADS1X1X_SPIN_LIMIT_US			1000UL

// Time (us) added to the wait of every single shot conversion for the device to wake up from power-down.
//#define PIF_ADS1X1X_WAKEUP_US				50UL


// -------- pifBasic -----------------------------

//#define PIF_BASIC_LINE_SIZE				80		// The max size of a line in a program
//#define PIF_BASIC_SYMBOL					16		// Maximum number of characters in the symbol
//#define PIF_BASIC_PROGRAM					2048	// Program size
//#define PIF_BASIC_STACK					64		// Stack size
//#define PIF_BASIC_STRING					1024	// String table size
//#define PIF_BASIC_VARIABLE				128		// Variable count
//#define PIF_BASIC_LOCAL					8		// Local count
//#define PIF_BASIC_EXEC_SIZE				4		// Maximum number of EXEC command parameters
//#define PIF_BASIC_OPCODE					32		// Count of opcodes processed at one time


// -------- pifBattery ---------------------------

//#define PIF_BATTERY_MAX_CELLS				12


// -------- pifBst -------------------------------

// Key width in bits: 8, 16 or 32.
//#define PIF_BST_KEY						32


// -------- pifCan -------------------------------

// Largest data of a frame: 64 for CAN FD, or 8 when only Classic CAN is used, which saves memory in every
// frame queue and stream.
//#define PIF_CAN_MAX_DATA_SIZE				64


// -------- pifCollectSignal ---------------------

//#define PIF_COLLECT_SIGNAL

// With PIF_COLLECT_SIGNAL every module below records its signals. Defining a module's NO_ macro
// leaves its channels, its Put calls and its SetCsFlag/ResetCsFlag out of the build.
//#define PIF_COLLECT_SIGNAL_NO_BUZZER
//#define PIF_COLLECT_SIGNAL_NO_DSHOT
//#define PIF_COLLECT_SIGNAL_NO_DUTY_MOTOR
//#define PIF_COLLECT_SIGNAL_NO_ENCODER
//#define PIF_COLLECT_SIGNAL_NO_GPIO
//#define PIF_COLLECT_SIGNAL_NO_HC_SR04
//#define PIF_COLLECT_SIGNAL_NO_I2C
//#define PIF_COLLECT_SIGNAL_NO_KEYPAD
//#define PIF_COLLECT_SIGNAL_NO_MAX31855
//#define PIF_COLLECT_SIGNAL_NO_MODBUS_ASCII_MASTER
//#define PIF_COLLECT_SIGNAL_NO_MODBUS_RTU_MASTER
//#define PIF_COLLECT_SIGNAL_NO_PID_CONTROL
//#define PIF_COLLECT_SIGNAL_NO_PULSE
//#define PIF_COLLECT_SIGNAL_NO_SENSOR_DIGITAL
//#define PIF_COLLECT_SIGNAL_NO_SENSOR_SWITCH
//#define PIF_COLLECT_SIGNAL_NO_SEQUENCE
//#define PIF_COLLECT_SIGNAL_NO_SOLENOID
//#define PIF_COLLECT_SIGNAL_NO_STEP_MOTOR		// pifStepMotorPos and pifStepMotorSpeed too
//#define PIF_COLLECT_SIGNAL_NO_UART

// Characters of text a transfer task pass hands to pifLog at most, beyond the line it is on.
//#define PIF_COLLECT_SIGNAL_TEXT_SIZE		96

// Bytes every channel keeps for its name, the terminating NUL included. A longer name is refused.
// pifSolenoid needs 7 for its names up to "SNA254".
//#define PIF_COLLECT_SIGNAL_NAME_SIZE		8

// -------- pifDshot -----------------------------

//#define PIF_DSHOT_MAX_MOTORS				8
//#define PIF_DSHOT_COMMAND_QUEUE_SIZE		3


// -------- pifDynNotch --------------------------

// Samples in the analysis window. The frequency resolution is the analysis rate divided by this.
//#define PIF_DYN_NOTCH_SDFT_SIZE			72
//#define PIF_DYN_NOTCH_MAX_NOTCHES			7


// -------- pifEncoder ---------------------------

// Quarter steps whose times are kept per PifEncoder for the speed, a power of two of at least 2. The speed
// averages over whole encoder cycles (4 quarter steps) once 5 are kept. Each one takes 4 bytes.
//#define PIF_ENCODER_DATA_SIZE				8


// -------- pifFlash -----------------------------

// Largest program unit (flash word) a PifFlash accepts: 4 on STM32F4, 8 on G4, 32 on H743.
//#define PIF_FLASH_MAX_PROGRAM_SIZE		32


// -------- pifGpio ------------------------------

// State width in bits and maximum pin count of one PifGpio: 8, 16 or 32.
//#define PIF_GPIO_WIDTH					8


// -------- pifGps -------------------------------

//#define PIF_GPS_NMEA_VALUE_SIZE			64
//#define PIF_GPS_NMEA_TEXT_SIZE			64

//#define PIF_GPS_SV_MAXSATS				16


// -------- pifGpsUblox --------------------------

//#define PIF_GPS_UBLOX_TX_SIZE				64

// Payload bytes a received UBX packet may carry. NAV-SVINFO with 32 channels sets the size unless
// this asks for more. Raise it to read NAV-SAT from a receiver tracking many satellites.
//#define PIF_GPS_UBLOX_RX_PAYLOAD_SIZE		1


// -------- pifHcSr04 ----------------------------

// How long (us) a measurement may wait for each edge of its echo before it is triggered again.
// The echo lasts at most about 38ms, which is what the sensor gives when nothing is in range.
//#define PIF_HC_SR04_ECHO_TIMEOUT_US		60000UL


// -------- pifKeypad ----------------------------

//#define PIF_KEYPAD_DEFAULT_HOLD_TIME		100
//#define PIF_KEYPAD_DEFAULT_LONG_TIME		1000
//#define PIF_KEYPAD_DEFAULT_DOUBLE_TIME	300


// -------- pifLink ------------------------------

// Largest data size of one received packet, without the header and CRC.
//#define PIF_LINK_RX_PACKET_SIZE			32

// Size of the ring buffer that holds the requests waiting to be sent or answered.
//#define PIF_LINK_TX_REQUEST_SIZE			64

// Size of the ring buffer that holds the answers and NAKs waiting to be sent.
//#define PIF_LINK_TX_ANSWER_SIZE			32

// Timeout used to receive one complete packet, in timer units. 0: no timeout limit.
//#define PIF_LINK_RECEIVE_TIMEOUT			50

// Delay before a request is sent again after a NAK or a broken response, in timer units. 0: at once.
//#define PIF_LINK_RETRY_DELAY				10


// -------- pifLinkFragment ----------------------

// Default largest data size of one fragment sent by pifLink_MakeLargeRequest(), without its fragment byte.
//#define PIF_LINK_TX_FRAGMENT_SIZE			(PIF_LINK_RX_PACKET_SIZE - 1)


// -------- pifLog -------------------------------

//#define PIF_NO_LOG
//#define PIF_LOG_COMMAND

//#define PIF_LOG_LINE_SIZE					80


// -------- pifModbus ----------------------------

// Time in ms a master waits for the response once the request is out, and for each next byte of it.
// Sending the request gets its own transfer time on top, so a long request does not use it up.
//#define PIF_MODBUS_MASTER_TIMEOUT 	    500

// Time in ms a slave waits for the next byte of a frame it has started to receive, before it drops
// the frame. It runs from byte to byte, so a long frame at a low baud rate is not cut short.
// An RTU slave also waits this long for another slave to answer the request sent to it, before it
// takes the next frame for a request again.
//#define PIF_MODBUS_SLAVE_TIMEOUT  	    300


// -------- pifMsp -------------------------------

// Size of the receive buffer of one packet. 0: the client gives one with pifMsp_AssignRxBuffer().
//#define PIF_MSP_RX_PACKET_SIZE			128

// Size of the answer ring buffer. 0: the client gives one with pifMsp_AssignAnswerBuffer().
//#define PIF_MSP_TX_ANSWER_SIZE			128

// Timeout used to receive one complete packet, in timer units. 0: no timeout limit.
//#define PIF_MSP_RECEIVE_TIMEOUT			200


// -------- pifPmlcdI2c --------------------------

// Size of the buffer pifPmlcdI2c_Printf() formats into, with the terminating null.
//#define PIF_PMLCD_I2C_PRINTF_SIZE			32


// -------- pifPulse -----------------------------

// Falling edges kept per PifPulse, a power of two of at least 2. pifPulse_GetAveragePeriod averages over
// one fewer periods than this. Each edge kept takes 8 bytes.
//#define PIF_PULSE_DATA_SIZE				4


// -------- pifPwmGen ----------------------------

//#define PIF_PWM_GEN_MAX_DUTY				1000


// -------- pifSequence --------------------------

// How often (us) a wait looks for its signal, which is the latency between the signal and the next step.
//#define PIF_SEQUENCE_WAIT_POLL_US			1000UL


// -------- pifSolenoid --------------------------

// Channels one PifSolenoid takes at most. Their state is allocated by pifSolenoid_Init for the count
// asked for, so this is only a bound, at most 255.
//#define PIF_SOLENOID_MAX_COUNT			16


// -------- pifSrml ------------------------------

//#define PIF_SRML_MAX_BUFFER_SIZE     		64


// -------- pifTask ------------------------------

// Margin in microseconds that a run has to leave free before the realtime release, on top of its
// own measured length. It stands for what the scheduler itself spends between deciding that the
// run fits and dispatching the release, which no measurement of the run can see.
// The margin is not fixed at the minimum: a release that turns out to have been late raises it and
// on time releases lower it again, between these two bounds. Raise the minimum only if the very
// first releases have to be on time as well, since the margin needs a few late ones to find its
// level. pifTaskManager_Print() reports where it settled, next to the number of runs that were let
// through although they do not fit.
//#define PIF_TASK_GUARD_MIN_US		        2
//#define PIF_TASK_GUARD_MAX_US		        100

// How many times in a row the margin above may hold a release back before it is let through
// anyway. The margin alone gives no bound: a run shorter than the realtime period but longer than
// the slack it happens to be offered can be refused on every visit, and nothing in the rule makes
// the next visit any more likely to succeed. With this, the wait a release can suffer is stated
// instead: at most this many passes of the ring while it is already due.
// It is a bound, not a target. Lower it and the wait tightens while realtime jitter grows, because
// more runs are let through against the margin; raise it and the opposite. Runs let through this
// way are counted with the ones that do not fit at all, which pifTaskManager_Print() reports as
// lapses, so raise it if lapses climb with no task whose measured run exceeds the whole period.
// PifTask::max_skip and PifTaskTimer::max_skip override it for one owner, where 0 means this
// value and 1 means never held back. The idle callback is not bounded: having no time left over
// is the answer for idle work, not a wait to cut short.
//#define PIF_TASK_MAX_SKIP			        10

//#define PIF_USE_TASK_STATISTICS

// Measures the longest single run of each task, over a moving window of the last 100 to 200 runs.
// That is what the realtime task can be delayed by, and TM_REALTIME uses it to skip a task that
// would not finish before the release. The timer and idle callbacks are measured and held back by
// the same rule. PIF_USE_TASK_STATISTICS enables it as well.
//#define PIF_USE_BLOCK_TIME


// -------- pifTftLcd ----------------------------

// Color depth: 16 (RGB565), 32 (XRGB8888)
#define PIF_COLOR_DEPTH 					16


// -------- pifTouchScreen -----------------------

//#define PIF_TOUCH_CONTROL_PERIOD			10

// Reads of the pressed state that have to agree before calibration accepts a press or a release.
//#define PIF_TOUCH_CALIBRATION_DEBOUNCE	10

// Samples of one crosshair that calibration accepts before it moves on to the next.
//#define PIF_TOUCH_CALIBRATION_SAMPLES		400

// Reads calibration takes per release of the task while sampling a crosshair.
//#define PIF_TOUCH_CALIBRATION_BATCH		40

// Reads of an unpressed panel that make calibration give up on a crosshair.
//#define PIF_TOUCH_CALIBRATION_MAX_FAIL	10000


// -------- pifUart ------------------------------

// Leaves out XON/XOFF and RTS/CTS flow control: fc_limit, act_get_rx_rate, act_device_flow_state and the
// pifUart_*FlowControl/*FlowState functions. The flow state of the UART transport is then always ON.
//#define PIF_UART_NO_FLOW_CONTROL

// Leaves out the direction control of a half-duplex line such as RS-485: dir_delay and
// pifUart_AttachActDirection/SetDirection. Without it the UART drives the line for each transfer by itself.
//#define PIF_UART_NO_DIRECTION


// -------- pifXmodem ----------------------------

// Time in ms to wait for ACK/NAK after sending one packet or EOT. The time to send the packet on
// the transport is added to it. pifXmodem_SetResponseTimeout() changes it per instance.
//#define PIF_XMODEM_RESPONSE_TIMEOUT		500

// Time in ms allowed to receive one complete packet. The time to receive the packet on the
// transport is added to it. pifXmodem_SetReceiveTimeout() changes it per instance.
//#define PIF_XMODEM_RECEIVE_TIMEOUT		300

// Quiet time in ms on the line that ends the purge after a bad packet, before the NAK is sent.
// Keep it longer than any gap the sender may leave between the bytes of a packet.
//#define PIF_XMODEM_PURGE_TIME				100

// Interval in ms at which the receiver repeats the start request until the first packet comes.
//#define PIF_XMODEM_START_INTERVAL			1000

// How many times EOT is sent again after a NAK or no answer before the transfer is given up.
//#define PIF_XMODEM_EOT_RETRY				3


#endif  // PIF_CONF_H
