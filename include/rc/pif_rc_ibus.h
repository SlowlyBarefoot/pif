#ifndef PIF_RC_IBUS_H
#define PIF_RC_IBUS_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_IBUS_CHANNEL_COUNT		14
#define PIF_IBUS_EXP_CHANNEL_COUNT	(PIF_IBUS_CHANNEL_COUNT + 4)

#define IBUS_COMMAND_SERVO          0x40    // Servo command.
#define IBUS_COMMAND_DISCOVER       0x80    // Sensor discovery command.
#define IBUS_COMMAND_TYPE           0x90    // Sensor type query command.
#define IBUS_COMMAND_VALUE          0xA0    // Sensor value query command.

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


/**
 * @brief iBUS telemetry sensor descriptor.
 */
typedef struct StPifRcIbusSensorinfo
{
    uint8_t type;          // Sensor type identifier.
    uint8_t length;        // Sensor payload length in bytes.
    uint8_t offset;
    uint8_t value[29];     // Sensor payload bytes.
} PifRcIbusSensorinfo;


typedef struct StPifRcIbus PifRcIbus;

/**
 * @brief Callback used to produce telemetry payload for iBUS sensor requests.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param command iBUS command code.
 * @param address Sensor address.
 * @param p_sensor Output descriptor containing type/length/value payload.
 */
typedef void (*PifEvtRcIbusTelemetry)(PifRcIbus* p_owner, uint8_t command, uint8_t address, PifRcIbusSensorinfo* p_sensor);


/**
 * @class StPifRcIbus
 * @brief Runtime state for the iBUS receiver and telemetry responder.
 */
struct StPifRcIbus
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

    // Public Event Function
	PifEvtRcIbusTelemetry evt_telemetry;

	// Read-only Member Variable
    PifRcIbusModel _model;
    uint8_t _number_sensors;  // Number of telemetry sensors.
    uint8_t _length;          // Current RX message length.

	// Private Member Variable
    PifUart* __p_uart;
    PifRcIbusRxState __rx_state;
    uint8_t __rx_buffer[IBUS_FRAME_SIZE];
    uint32_t __last_time;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcIbus_Init
 * @brief Initializes an iBUS receciver instance.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcIbus_Init(PifRcIbus* p_owner, PifId id);

/**
 * @fn pifRcIbus_Clear
 * @brief Clears iBUS runtime state.
 * @param p_owner Pointer to the iBUS receiver object.
 */
void pifRcIbus_Clear(PifRcIbus* p_owner);

/**
 * @fn pifRcIbus_AttachUart
 * @brief Attaches a UART interface to the iBUS parser.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcIbus_AttachUart(PifRcIbus* p_owner, PifUart* p_uart);

/**
 * @fn pifRcIbus_DetachUart
 * @brief Detaches the UART interface from the iBUS parser.
 * @param p_owner Pointer to the iBUS receiver object.
 */
void pifRcIbus_DetachUart(PifRcIbus* p_owner);

/**
 * @fn pifRcIbus_SendFrame
 * @brief Encodes and transmits one iBUS servo frame.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param p_channel Pointer to channel values.
 * @param count Number of channel values available in p_channel.
 * @return TRUE if the frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcIbus_SendFrame(PifRcIbus* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_IBUS_H
