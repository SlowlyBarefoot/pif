// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_CONF_H
#define PIF_CONF_H


// -------- pif Configuration --------------------

//#define PIF_DEBUG

//#define PIF_INLINE
#define PIF_INLINE                      	inline
//#define PIF_INLINE                      	__inline

//#define PIF_WEAK							__attribute__ ((weak))


// -------- pifAdc -------------------------------

//#define PIF_ADC_MAX_CHANNELS				8


// -------- pifAds1x1x ---------------------------

// Conversions no longer than this (us) are waited out with the CPU held instead of through the timer.
//#define PIF_ADS1X1X_SPIN_LIMIT_US			1000UL


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


// -------- pifCollectSignal ---------------------

//#define PIF_COLLECT_SIGNAL


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

// Timeout used after sending one packet while waiting for a response.
// This value is multiplied by the timer unit configured in pifModbus[Rtu/Ascii]Master_Init().
// Default is 500 ticks, which equals 500 ms when the timer unit is 1 ms.
//#define PIF_MODBUS_MASTER_TIMEOUT 	    500

// Timeout used to receive one complete packet.
// This value is multiplied by the timer unit configured in pifModbus[Rtu/Ascii]Slave_Init().
// Default is 300 ticks, which equals 300 ms when the timer unit is 1 ms.
//#define PIF_MODBUS_SLAVE_TIMEOUT  	    300


// -------- pifMsp -------------------------------

// Size of the receive buffer of one packet. 0: the client gives one with pifMsp_AssignRxBuffer().
//#define PIF_MSP_RX_PACKET_SIZE			128

// Size of the answer ring buffer. 0: the client gives one with pifMsp_AssignAnswerBuffer().
//#define PIF_MSP_TX_ANSWER_SIZE			128

// Timeout used to receive one complete packet, in timer units. 0: no timeout limit.
//#define PIF_MSP_RECEIVE_TIMEOUT			200


// -------- pifPulse -----------------------------

// Falling edges kept per PifPulse, a power of two of at least 2. pifPulse_GetAveragePeriod averages over
// one fewer periods than this. Each edge kept takes 8 bytes.
//#define PIF_PULSE_DATA_SIZE				4


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


// -------- pifTimer -----------------------------

//#define PIF_PWM_MAX_DUTY					1000


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


#endif  // PIF_CONF_H
