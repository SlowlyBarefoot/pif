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
//#define PIF_LOG_RX_BUFFER_SIZE			32
//#define PIF_LOG_TX_BUFFER_SIZE			80


// -------- pifModbus ----------------------------

// 한 packet을 보내고 응답을 받는 시간 제한
// pifModbus[Rtu/Ascii]Master_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 500이고 타이머 단위가 1ms이기에 500 * 1ms = 500ms이다.
//#define PIF_MODBUS_MASTER_TIMEOUT 	    500

// 한 packet을 전부 받는 시간 제한
// pifModbus[Rtu/Ascii]Slave_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 300이고 타이머 단위가 1ms이기에 300 * 1ms = 300ms이다.
//#define PIF_MODBUS_SLAVE_TIMEOUT  	    300


// -------- pifProtocol --------------------------

//#define PIF_PROTOCOL_RX_PACKET_SIZE		32
//#define PIF_PROTOCOL_TX_REQUEST_SIZE		64
//#define PIF_PROTOCOL_TX_ANSWER_SIZE		32

// 한 packet을 전부 받는 시간 제한
// 0 : 제한없음
// 1이상 : pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 50이고 타이머 단위가 1ms이면 50 * 1ms = 50ms이다.
//#define PIF_PROTOCOL_RECEIVE_TIMEOUT		50

// Retry하기 전 delay 시간
// 0 : 제한없음
// 1이상 : pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 10이고 타이머 단위가 1ms이면 10 * 1ms = 10ms이다.
//#define PIF_PROTOCOL_RETRY_DELAY			10


// -------- pifSrml ------------------------------

//#define PIF_SRML_MAX_BUFFER_SIZE     		64


// -------- pifTask ------------------------------

//#define PIF_TASK_TABLE_SIZE				32


// -------- pifTftLcd ----------------------------

// Color depth: 16 (RGB565), 32 (XRGB8888)
#define PIF_COLOR_DEPTH 					16


// -------- pifTimer -----------------------------

//#define PIF_PWM_MAX_DUTY					1000


#endif  // PIF_CONF_H
