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
	IRS_GET_CHKSUMH	= 4
} PifRcIbusRxState;

/**
 * @brief What a byte given to pifRcIbus_ParsingPacket() completed.
 */
typedef enum EnPifRcIbusFrame
{
	IBUS_FRAME_NONE			= 0,	// No frame, or one that is not for us.
	IBUS_FRAME_SERVO		= 1,	// A servo frame; its channels have gone to evt_receive.
	IBUS_FRAME_TELEMETRY	= 2,	// A sensor request; see _tlm_command and _tlm_address.
} PifRcIbusFrame;


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
 * @brief Callback used to answer iBUS sensor requests.
 *        It is called for IBUS_COMMAND_DISCOVER, IBUS_COMMAND_TYPE and IBUS_COMMAND_VALUE. For
 *        IBUS_COMMAND_TYPE it fills type and length, for IBUS_COMMAND_VALUE length and value; for
 *        IBUS_COMMAND_DISCOVER nothing is needed.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param command iBUS command code.
 * @param address Sensor address.
 * @param p_sensor Output descriptor containing type/length/value payload.
 * @return TRUE to send the reply, FALSE to leave the address unanswered.
 */
typedef BOOL (*PifEvtRcIbusTelemetry)(PifRcIbus* p_owner, uint8_t command, uint8_t address, PifRcIbusSensorinfo* p_sensor);


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
    uint8_t _length;          // Current RX message length.
    uint8_t _tlm_command;     // Command of the last sensor request received.
    uint8_t _tlm_address;     // Sensor address of the last sensor request received.

	// Private Member Variable
    PifUart* __p_uart;
    PifRcIbusRxState __rx_state;
    uint8_t __rx_buffer[IBUS_FRAME_SIZE];
    uint32_t __last_time;
	uint8_t __ptr;                      // pointer in buffer
	uint16_t __chksum;                  // checksum calculation
	uint8_t __lchksum;                  // checksum lower byte received
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
 * @fn pifRcIbus_ParsingPacket
 * @brief Feeds one received byte to the iBUS parser, as the UART receive callback does. For a
 *        client that gets the bytes some other way than through an attached PifUart, for example
 *        from the receive interrupt. The channels of a servo frame go to evt_receive from here, so
 *        called from an interrupt, evt_receive runs in it too. A sensor request is not answered:
 *        IBUS_FRAME_TELEMETRY is returned, and the client answers it with
 *        pifRcIbus_SendTelemetry() once it can send.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param data Received byte.
 * @return What the byte completed.
 */
PifRcIbusFrame pifRcIbus_ParsingPacket(PifRcIbus* p_owner, uint8_t data);

/**
 * @fn pifRcIbus_SendTelemetry
 * @brief Answers a sensor request: gets the sensor from evt_telemetry, then builds the reply and
 *        sends it through the attached PifUart. The attached UART path calls this itself.
 * @param p_owner Pointer to the iBUS receiver object.
 * @param command Command of the request (IBUS_COMMAND_DISCOVER, _TYPE or _VALUE).
 * @param address Sensor address of the request.
 * @return TRUE if a reply was sent, FALSE if evt_telemetry declined the address, the command is
 *         unknown or there is no UART to send with.
 */
BOOL pifRcIbus_SendTelemetry(PifRcIbus* p_owner, uint8_t command, uint8_t address);

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
