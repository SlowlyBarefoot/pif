#ifndef PIF_CONF_H
#define PIF_CONF_H


// -------- pif Configuration --------------------

//#define PIF_DEBUG

//#define PIF_INLINE
#define PIF_INLINE                      	inline
//#define PIF_INLINE                      	__inline

//#define PIF_WEAK							__attribute__ ((weak))


// -------- pifCollectSignal ---------------------

//#define PIF_COLLECT_SIGNAL


// -------- pifGpsNmea ---------------------------

//#define PIF_GPS_NMEA_VALUE_SIZE			32
//#define PIF_GPS_NMEA_TEXT_SIZE			64


// -------- pifKeypad ----------------------------

//#define PIF_KEYPAD_DEFAULT_HOLD_TIME		100
//#define PIF_KEYPAD_DEFAULT_LONG_TIME		1000
//#define PIF_KEYPAD_DEFAULT_DOUBLE_TIME	300


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


// -------- pifSrml ------------------------------

//#define PIF_SRML_MAX_BUFFER_SIZE     		64


// -------- pifTask ------------------------------

//#define PIF_TASK_STACK_SIZE		        5

#define DISALLOW_YIELD_ID_NONE		        0
#define DISALLOW_YIELD_ID_I2C		        1
#define DISALLOW_YIELD_ID_SPI		        2

//#define PIF_USE_TASK_STATISTICS

// Measures the longest run of each task without yielding, over a moving window of the last 100 to
// 200 runs. That is what the realtime task can be delayed by, and TM_REALTIME uses it to skip a
// task that would not finish before the release. The timer and idle callbacks are measured and
// held back by the same rule. PIF_USE_TASK_STATISTICS enables it as well.
//#define PIF_USE_BLOCK_TIME

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


// -------- pifTftLcd ----------------------------

// Color depth: 16 (RGB565), 32 (XRGB8888)
#define PIF_COLOR_DEPTH 					16


// -------- pifTimer -----------------------------

//#define PIF_PWM_MAX_DUTY					1000


#endif  // PIF_CONF_H
