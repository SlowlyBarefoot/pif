// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_RC_CRSF_H
#define PIF_RC_CRSF_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


/*
 * CRSF (Crossfire serial protocol), the link between a receiver and a flight controller, or a
 * radio and its transmitter module, used by TBS Crossfire and Tracer and by ExpressLRS.
 *
 * Every frame is
 *     [address] [length] [type] [payload ...] [CRC8]
 * where length counts the type, the payload and the CRC (2 to 62), and the CRC is CRC-8/DVB-S2
 * (polynomial 0xD5) over the type and the payload. A frame is at most 64 bytes. Multi-byte fields
 * are big-endian. Frame types from 0x28 up are extended frames: their payload starts with the
 * destination and the origin address. The UART runs at 420000 baud, 8N1, by default.
 *
 * Received RC channel frames go to the evt_receive of PifRc as 16 raw 11-bit values, 172 to 1811
 * with 992 at the center; pifRcCrsf_RawToUs() turns one into microseconds. Link statistics are
 * kept in _link and reported through evt_link. Any other frame, such as a device ping, a parameter
 * request or MSP, goes to evt_frame for the client to handle. The protocol has no failsafe flag:
 * a receiver that loses the link stops sending channels, which pifRc_CheckFailSafe() sees after
 * _max_frame_period.
 *
 * The frame builders (pifRcCrsf_Build...) write a frame into a buffer; the Send functions build
 * one and send it through the attached UART.
 */


#define PIF_CRSF_BAUDRATE				420000
#define PIF_CRSF_CHANNEL_COUNT			16
#define PIF_CRSF_FRAME_SIZE_MAX			64
#define PIF_CRSF_PAYLOAD_SIZE_MAX		(PIF_CRSF_FRAME_SIZE_MAX - 4)	// Without address, length, type and CRC

// Raw channel values: 172 is 988 us, 992 is 1500 us, 1811 is 2012 us.
#define PIF_CRSF_CHANNEL_MIN			172
#define PIF_CRSF_CHANNEL_CENTER			992
#define PIF_CRSF_CHANNEL_MAX			1811

// Device addresses
#define PIF_CRSF_ADDRESS_BROADCAST				0x00
#define PIF_CRSF_ADDRESS_USB					0x10
#define PIF_CRSF_ADDRESS_CURRENT_SENSOR			0xC0
#define PIF_CRSF_ADDRESS_GPS					0xC2
#define PIF_CRSF_ADDRESS_FLIGHT_CONTROLLER		0xC8	// Also the sync byte of frames to a flight controller
#define PIF_CRSF_ADDRESS_RADIO_TRANSMITTER		0xEA	// The radio (handset)
#define PIF_CRSF_ADDRESS_RECEIVER				0xEC
#define PIF_CRSF_ADDRESS_TRANSMITTER			0xEE	// The transmitter module

// Frame types
#define PIF_CRSF_FRAME_GPS						0x02
#define PIF_CRSF_FRAME_VARIO					0x07
#define PIF_CRSF_FRAME_BATTERY					0x08
#define PIF_CRSF_FRAME_LINK_STATISTICS			0x14
#define PIF_CRSF_FRAME_RC_CHANNELS				0x16
#define PIF_CRSF_FRAME_ATTITUDE					0x1E
#define PIF_CRSF_FRAME_FLIGHT_MODE				0x21
#define PIF_CRSF_FRAME_EXTENDED_FIRST			0x28	// Types from here on carry destination and origin
#define PIF_CRSF_FRAME_DEVICE_PING				0x28
#define PIF_CRSF_FRAME_DEVICE_INFO				0x29
#define PIF_CRSF_FRAME_PARAMETER_ENTRY			0x2B
#define PIF_CRSF_FRAME_PARAMETER_READ			0x2C
#define PIF_CRSF_FRAME_PARAMETER_WRITE			0x2D
#define PIF_CRSF_FRAME_COMMAND					0x32
#define PIF_CRSF_FRAME_RADIO_ID					0x3A
#define PIF_CRSF_FRAME_MSP_REQUEST				0x7A
#define PIF_CRSF_FRAME_MSP_RESPONSE				0x7B
#define PIF_CRSF_FRAME_MSP_WRITE				0x7C

// A receiver that stops sending channels for this long has lost the link.
#define PIF_CRSF_DEFAULT_TIMEOUT_MS		250


/**
 * @brief What a byte given to pifRcCrsf_ParsingPacket() completed.
 */
typedef enum EnPifRcCrsfFrame
{
	CRSF_FRAME_NONE			= 0,	// No frame yet, or one with a bad CRC.
	CRSF_FRAME_CHANNELS		= 1,	// RC channels; they have gone to evt_receive.
	CRSF_FRAME_LINK			= 2,	// Link statistics; they are in _link.
	CRSF_FRAME_OTHER		= 3		// Any other frame; it has gone to evt_frame.
} PifRcCrsfFrame;

/**
 * @brief Link statistics as the receiver reports them.
 */
typedef struct StPifRcCrsfLink
{
	uint8_t uplink_rssi_ant1;		// -dBm of antenna 1
	uint8_t uplink_rssi_ant2;		// -dBm of antenna 2
	uint8_t uplink_link_quality;	// Packets received, percent
	int8_t uplink_snr;				// dB
	uint8_t active_antenna;
	uint8_t rf_mode;				// Packet rate, as an index the transmitter defines
	uint8_t uplink_tx_power;		// Transmitter power, as an index the transmitter defines
	uint8_t downlink_rssi;			// -dBm
	uint8_t downlink_link_quality;	// Percent
	int8_t downlink_snr;			// dB
} PifRcCrsfLink;


typedef struct StPifRcCrsf PifRcCrsf;

/**
 * @brief Reports new link statistics.
 * @param p_owner Pointer to the CRSF instance. The statistics are in _link.
 */
typedef void (*PifEvtRcCrsfLink)(PifRcCrsf* p_owner);

/**
 * @brief Reports a frame other than RC channels and link statistics.
 * @param p_owner Pointer to the CRSF instance.
 * @param address First byte of the frame.
 * @param type Frame type.
 * @param p_payload Payload; for an extended frame it starts with the destination and the origin.
 * @param length Payload length.
 */
typedef void (*PifEvtRcCrsfFrame)(PifRcCrsf* p_owner, uint8_t address, uint8_t type, const uint8_t* p_payload, uint8_t length);


/**
 * @class StPifRcCrsf
 * @brief CRSF receiver and telemetry sender.
 */
struct StPifRcCrsf
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Public Event Function
	PifEvtRcCrsfLink evt_link;
	PifEvtRcCrsfFrame evt_frame;

	// Read-only Member Variable
	PifRcCrsfLink _link;
	uint8_t _address;					// First byte of the frames sent, PIF_CRSF_ADDRESS_FLIGHT_CONTROLLER by default

	// Private Member Variable
	PifUart* __p_uart;
	uint8_t __rx_buffer[PIF_CRSF_FRAME_SIZE_MAX];
	uint8_t __ptr;						// Bytes of the frame received so far; 0 between frames
	uint32_t __last_time;				// pif_cumulative_timer1ms of the last byte
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcCrsf_Init
 * @brief Initializes a CRSF instance.
 * @param p_owner Pointer to the instance.
 * @param id Object identifier, or PIF_ID_AUTO.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRcCrsf_Init(PifRcCrsf* p_owner, PifId id);

/**
 * @fn pifRcCrsf_Clear
 * @brief Clears the instance.
 * @param p_owner Pointer to the instance.
 */
void pifRcCrsf_Clear(PifRcCrsf* p_owner);

/**
 * @fn pifRcCrsf_AttachUart
 * @brief Attaches the UART the frames come in and go out through.
 * @param p_owner Pointer to the instance.
 * @param p_uart UART instance, set to PIF_CRSF_BAUDRATE 8N1 by the client.
 */
void pifRcCrsf_AttachUart(PifRcCrsf* p_owner, PifUart* p_uart);

/**
 * @fn pifRcCrsf_DetachUart
 * @brief Detaches the UART.
 * @param p_owner Pointer to the instance.
 */
void pifRcCrsf_DetachUart(PifRcCrsf* p_owner);

/**
 * @fn pifRcCrsf_SetAddress
 * @brief Sets the first byte of the frames sent: PIF_CRSF_ADDRESS_FLIGHT_CONTROLLER (the default)
 *        for telemetry from a flight controller to its receiver, PIF_CRSF_ADDRESS_TRANSMITTER from
 *        a radio to its transmitter module.
 * @param p_owner Pointer to the instance.
 * @param address First byte of the frames sent.
 */
void pifRcCrsf_SetAddress(PifRcCrsf* p_owner, uint8_t address);

/**
 * @fn pifRcCrsf_SetTimeout
 * @brief Sets how long without RC channels counts as a lost link for pifRc_CheckFailSafe().
 * @param p_owner Pointer to the instance.
 * @param timeout_ms Time in milliseconds, or 0 for no timeout.
 */
void pifRcCrsf_SetTimeout(PifRcCrsf* p_owner, uint16_t timeout_ms);

/**
 * @fn pifRcCrsf_ParsingPacket
 * @brief Feeds one received byte to the parser, as the UART receive callback does, for a client
 *        that gets the bytes another way, for example from the receive interrupt. Events are
 *        called from here.
 * @param p_owner Pointer to the instance.
 * @param data Received byte.
 * @return What the byte completed.
 */
PifRcCrsfFrame pifRcCrsf_ParsingPacket(PifRcCrsf* p_owner, uint8_t data);

/**
 * @fn pifRcCrsf_RawToUs
 * @brief Converts a raw channel value to a pulse width.
 * @param raw Raw channel value, 172 to 1811 in normal use.
 * @return Pulse width in microseconds, 1500 + (raw - 992) * 5 / 8 rounded: 988 for 172, 2012 for 1811.
 */
uint16_t pifRcCrsf_RawToUs(uint16_t raw);

/**
 * @fn pifRcCrsf_UsToRaw
 * @brief Converts a pulse width to a raw channel value.
 * @param us Pulse width in microseconds.
 * @return Raw channel value, 992 + (us - 1500) * 8 / 5 rounded, within 0 to 2047.
 */
uint16_t pifRcCrsf_UsToRaw(uint16_t us);

/**
 * @fn pifRcCrsf_BuildFrame
 * @brief Writes a frame into a buffer.
 * @param p_buffer Buffer of at least length + 4 bytes.
 * @param address First byte of the frame.
 * @param type Frame type.
 * @param p_payload Payload, or NULL if length is 0.
 * @param length Payload length, up to PIF_CRSF_PAYLOAD_SIZE_MAX.
 * @return Frame size in bytes, or 0 if the payload is too long.
 */
uint8_t pifRcCrsf_BuildFrame(uint8_t* p_buffer, uint8_t address, uint8_t type, const uint8_t* p_payload, uint8_t length);

/**
 * @fn pifRcCrsf_BuildChannels
 * @brief Writes an RC channels frame into a buffer.
 * @param p_buffer Buffer of at least 26 bytes.
 * @param address First byte of the frame.
 * @param p_channel Raw channel values; those past count are sent as 0.
 * @param count Number of values in p_channel.
 * @return Frame size in bytes.
 */
uint8_t pifRcCrsf_BuildChannels(uint8_t* p_buffer, uint8_t address, const uint16_t* p_channel, uint8_t count);

/**
 * @fn pifRcCrsf_SendFrame
 * @brief Builds a frame with _address and sends it.
 * @param p_owner Pointer to the instance.
 * @param type Frame type.
 * @param p_payload Payload, or NULL if length is 0.
 * @param length Payload length, up to PIF_CRSF_PAYLOAD_SIZE_MAX.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendFrame(PifRcCrsf* p_owner, uint8_t type, const uint8_t* p_payload, uint8_t length);

/**
 * @fn pifRcCrsf_SendExtendedFrame
 * @brief Builds an extended frame (types from 0x28) with _address and sends it.
 * @param p_owner Pointer to the instance.
 * @param type Frame type.
 * @param destination Address of the device the frame is for.
 * @param origin Address of this device.
 * @param p_payload Payload after the two addresses, or NULL if length is 0.
 * @param length Payload length, up to PIF_CRSF_PAYLOAD_SIZE_MAX - 2.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendExtendedFrame(PifRcCrsf* p_owner, uint8_t type, uint8_t destination, uint8_t origin,
		const uint8_t* p_payload, uint8_t length);

/**
 * @fn pifRcCrsf_SendChannels
 * @brief Sends an RC channels frame, from a radio to its transmitter module or for a test.
 * @param p_owner Pointer to the instance.
 * @param p_channel Raw channel values.
 * @param count Number of values in p_channel.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendChannels(PifRcCrsf* p_owner, const uint16_t* p_channel, uint8_t count);

/**
 * @fn pifRcCrsf_SendBattery
 * @brief Sends battery telemetry.
 * @param p_owner Pointer to the instance.
 * @param voltage_dv Voltage in 0.1 V.
 * @param current_da Current in 0.1 A.
 * @param consumed_mah Charge drawn in mAh, up to 0xFFFFFF.
 * @param remaining_percent Remaining charge in percent.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendBattery(PifRcCrsf* p_owner, uint16_t voltage_dv, uint16_t current_da, uint32_t consumed_mah,
		uint8_t remaining_percent);

/**
 * @fn pifRcCrsf_SendGps
 * @brief Sends GPS telemetry.
 * @param p_owner Pointer to the instance.
 * @param latitude Latitude in 1e-7 degrees.
 * @param longitude Longitude in 1e-7 degrees.
 * @param groundspeed Ground speed in 0.1 km/h.
 * @param heading Course over ground in 0.01 degrees.
 * @param altitude_m Altitude in metres, -1000 to 64535.
 * @param satellites Number of satellites.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendGps(PifRcCrsf* p_owner, int32_t latitude, int32_t longitude, uint16_t groundspeed, uint16_t heading,
		int32_t altitude_m, uint8_t satellites);

/**
 * @fn pifRcCrsf_SendAttitude
 * @brief Sends attitude telemetry.
 * @param p_owner Pointer to the instance.
 * @param pitch Pitch in 0.0001 rad.
 * @param roll Roll in 0.0001 rad.
 * @param yaw Yaw in 0.0001 rad.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendAttitude(PifRcCrsf* p_owner, int16_t pitch, int16_t roll, int16_t yaw);

/**
 * @fn pifRcCrsf_SendFlightMode
 * @brief Sends the flight mode as text.
 * @param p_owner Pointer to the instance.
 * @param p_mode NUL-terminated text, cut to fit a frame.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendFlightMode(PifRcCrsf* p_owner, const char* p_mode);

/**
 * @fn pifRcCrsf_SendVario
 * @brief Sends the vertical speed.
 * @param p_owner Pointer to the instance.
 * @param vertical_speed Vertical speed in cm/s, positive up.
 * @return TRUE if the whole frame was queued, otherwise FALSE.
 */
BOOL pifRcCrsf_SendVario(PifRcCrsf* p_owner, int16_t vertical_speed);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_CRSF_H
