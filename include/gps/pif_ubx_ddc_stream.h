// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ubx_ddc_stream.h
 * @brief Byte stream to a u-blox receiver on I2C, through its DDC port, as a PifTransport.
 *
 * The receiver keeps the bytes it has for the host and tells how many there are in two registers, high byte
 * first, at 0xFD and 0xFE. The stream reads that count, then the bytes with plain reads that follow it, and
 * gives them to the client. What the client writes goes to the receiver as plain writes, which hold the task
 * until they are over.
 *
 * PifUbxDdcStream puts together what that takes: the I2C device, a PifDeviceStream and the DDC protocol it
 * runs. Attach a client, such as pif_gps_nmea or pif_gps_ublox, to &stream.stream.parent:
 *
 *     pifUbxDdcStream_Init(&s_gps_i2c, PIF_ID_AUTO, &g_i2c_port, 0x42, 128, 64, 500);
 *     pifGpsUblox_AttachTransport(&s_gps_ublox, &s_gps_i2c.stream.parent);
 */
#ifndef PIF_UBX_DDC_STREAM_H
#define PIF_UBX_DDC_STREAM_H


#include "communication/pif_device_stream.h"
#include "communication/pif_i2c.h"


/**
 * @class StPifUbxDdcProtocol
 * @brief The DDC protocol a PifUbxDdcStream runs. Only for the stream: a client does not use it.
 */
typedef struct StPifUbxDdcProtocol
{
	PifDeviceStreamProtocol parent;

	// Private Member Variable
	uint8_t __count_reg;				// First of the two count registers.
	uint16_t __pending;					// Bytes the receiver holds that are not read yet.
	uint16_t __read_size;				// Bytes asked for by the read that is running.
	uint8_t __count[2];
	uint8_t __phase;					// What the read that is running reads.
} PifUbxDdcProtocol;

/**
 * @class StPifUbxDdcStream
 * @brief Byte stream to a u-blox receiver on I2C. Attach a client to &stream.parent.
 */
typedef struct StPifUbxDdcStream
{
	// Public Member Variable
	PifDeviceStream stream;				// Its task is stream._p_task, whose period is the poll period.

	// Read-only Member Variable
	PifI2cDevice *_p_device;			// Set its parent.timeout and parent.max_transfer_size as the port needs.

	// Private Member Variable
	PifUbxDdcProtocol __protocol;
} PifUbxDdcStream;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifUbxDdcStream_Init
 * @brief Adds the receiver to the I2C port and initializes the stream, whose task waits until a client is
 *        attached.
 * @param p_owner Pointer to the stream.
 * @param id Logical ID of the stream, or PIF_ID_AUTO.
 * @param p_port I2C port the receiver is on.
 * @param addr 7-bit address of the receiver, 0x42 by default on u-blox receivers.
 * @param rx_size Most bytes taken from the receiver in one read. max_transfer_size of the device also limits
 *        a read when it is set.
 * @param tx_size Most bytes written to the receiver in one write. 0 for a client that never writes.
 * @param period1ms How often the receiver is asked for its count, in ms.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifUbxDdcStream_Init(PifUbxDdcStream *p_owner, PifId id, PifI2cPort *p_port, uint8_t addr, uint16_t rx_size,
		uint16_t tx_size, uint16_t period1ms);

/**
 * @fn pifUbxDdcStream_Clear
 * @brief Detaches the client, waits for a read that is running, then removes the task and the device and
 *        frees the buffers.
 * @param p_owner Pointer to the stream.
 */
void pifUbxDdcStream_Clear(PifUbxDdcStream *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_UBX_DDC_STREAM_H
