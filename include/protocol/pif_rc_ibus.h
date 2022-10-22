#ifndef PIF_RC_IBUS_H
#define PIF_RC_IBUS_H


#include "core/pif_comm.h"
#include "protocol/pif_rc.h"


#define PIF_IBUS_CHANNEL_COUNT	14
#define PIF_IBUS_SENSOR_MAX		10		// Max number of sensors

#define IBUS_FRAME_SIZE			0x20


typedef enum EnPifRcIbusRxState
{
	IRS_GET_LENGTH	= 0,
	IRS_GET_DATA	= 1,
	IRS_GET_CHKSUML	= 2,
	IRS_GET_CHKSUMH	= 3,
	IRS_DONE		= 4
} PifRcIbusRxState;


typedef struct StPifRcIbusSensorinfo 
{
	uint8_t type;             // sensor type (0,1,2,3, etc)
	uint8_t length;           // data length for defined sensor (can be 2 or 4)
	int32_t value;            // sensor data for defined sensors (16 or 32 bits)
} PifRcIbusSensorinfo;

/**
 * @class StPifRcIbus
 * @brief
 */
typedef struct StPifRcIbus
{
	PifRc parent;

	// Public Member Variable

    // Public Event Function
	PifEvtRcReceive evt_receive;

	// Read-only Member Variable
	uint8_t _number_sensors;					// number of sensors

	// Private Member Variable
	PifComm* __p_comm;
	PifRcIbusSensorinfo __sensors[PIF_IBUS_SENSOR_MAX];
	PifRcIbusRxState __rx_state;
	uint8_t __rx_length;                      	// rx message length
	uint8_t __rx_buffer[IBUS_FRAME_SIZE];		// rx message buffer
	uint32_t __last_time;
} PifRcIbus;


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
 * @fn pifRcIbus_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifRcIbus_AttachComm(PifRcIbus* p_owner, PifComm* p_comm);

/**
 * @fn pifRcIbus_DetachComm
 * @brief
 * @param p_owner
 */
void pifRcIbus_DetachComm(PifRcIbus* p_owner);

/**
 * @fn pifRcIbus_AddSensor
 * @brief
 * @param p_owner
 * @param type
 * @param len
 * @return
 */
BOOL pifRcIbus_AddSensor(PifRcIbus* p_owner, uint8_t type, uint8_t len);

/**
 * @fn pifRcIbus_SetSensorMeasurement
 * @brief
 * @param p_owner
 * @param adr
 * @param value
 */
void pifRcIbus_SetSensorMeasurement(PifRcIbus* p_owner, uint8_t adr, int32_t value);

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
