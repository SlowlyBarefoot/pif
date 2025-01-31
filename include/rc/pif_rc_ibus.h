#ifndef PIF_RC_IBUS_H
#define PIF_RC_IBUS_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_IBUS_CHANNEL_COUNT		14
#define PIF_IBUS_EXP_CHANNEL_COUNT	(PIF_IBUS_CHANNEL_COUNT + 4)

#define IBUS_COMMAND_SERVO			0x40	// Command to set servo or motor speed is always 0x40
#define IBUS_COMMAND_DISCOVER 		0x80 	// Command discover sensor (lowest 4 bits are sensor)
#define IBUS_COMMAND_TYPE 			0x90    // Command sensor type (lowest 4 bits are sensor)
#define IBUS_COMMAND_VALUE 			0xA0	// Command send sensor data (lowest 4 bits are sensor)

#define IBUS_FRAME_SIZE				0x20
#define IBUS_TELEMETRY_SIZE			0x04


typedef enum EnPifRcIbusModel
{
	IBUS_MODEL_IA6B	= 0,
	IBUS_MODEL_IA6	= 1,
} PifRcIbusModel;

typedef enum EnPifRcIbusRxState
{
	IRS_GET_LENGTH	= 0,
	IRS_GET_COMMAND	= 1,
	IRS_GET_DATA	= 2,
	IRS_GET_CHKSUML	= 3,
	IRS_GET_CHKSUMH	= 4,
	IRS_DONE		= 5
} PifRcIbusRxState;


typedef struct StPifRcIbusSensorinfo 
{
	uint8_t type;             // sensor type (0,1,2,3, etc)
	uint8_t length;           // data length for defined sensor (can be 2 or 4)
	uint8_t offset;
	uint8_t value[29];        // sensor data for defined sensors (16 or 32 bits)
} PifRcIbusSensorinfo;


typedef struct StPifRcIbus PifRcIbus;

typedef void (*PifEvtRcIbusTelemetry)(PifRcIbus *p_owner, uint8_t command, uint8_t address, PifRcIbusSensorinfo *p_sensor);


/**
 * @class StPifRcIbus
 * @brief
 */
struct StPifRcIbus
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

    // Public Event Function
	PifEvtRcIbusTelemetry evt_telemetry;

	// Read-only Member Variable
	PifRcIbusModel _model;
	uint8_t _number_sensors;					// number of sensors
	uint8_t _length;                      		// rx message length

	// Private Member Variable
	PifUart* __p_uart;
	PifRcIbusRxState __rx_state;
	uint8_t __rx_buffer[IBUS_FRAME_SIZE];		// rx message buffer
	uint32_t __last_time;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcIbus_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifRcIbus_Init(PifRcIbus* p_owner, PifId id);

/**
 * @fn pifRcIbus_Clear
 * @brief
 * @param p_owner
 */
void pifRcIbus_Clear(PifRcIbus* p_owner);

/**
 * @fn pifRcIbus_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifRcIbus_AttachUart(PifRcIbus* p_owner, PifUart* p_uart);

/**
 * @fn pifRcIbus_DetachUart
 * @brief
 * @param p_owner
 */
void pifRcIbus_DetachUart(PifRcIbus* p_owner);

/**
 * @fn pifRcIbus_SendFrame
 * @brief
 * @param p_owner
 * @param p_channel
 * @param count
 * @return
 */
BOOL pifRcIbus_SendFrame(PifRcIbus* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_IBUS_H
