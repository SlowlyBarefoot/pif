// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_device_reg.h
 * @brief Device on a bus that is read and written by internal address (register or memory offset), whatever
 *        the bus is.
 *
 * A bus driver, such as pif_i2c or pif_spi, embeds a PifDeviceReg as the first member of its device, named
 * parent, and calls pifDeviceReg_Init() for it. A client, such as a sensor driver, keeps a PifDeviceReg pointer and
 * uses only the pifDeviceReg_ functions, so the same client works on every bus.
 */
#ifndef PIF_DEVICE_REG_H
#define PIF_DEVICE_REG_H


#include "core/pif.h"


/**
 * @brief Bit mask of a register field, in its place in the register.
 */
typedef uint16_t PifRegMask;

/**
 * @brief State of a read started with pifDeviceReg_StartRead().
 */
typedef enum EnPifDeviceRegState
{
	DRS_IDLE,	///< No transfer started
	DRS_RUN,	///< Transfer in progress
	DRS_COMPLETE,	///< Transfer done and the data is in the buffer
	DRS_ERROR	///< Transfer failed
} PifDeviceRegState;

struct StPifDeviceReg;
typedef struct StPifDeviceReg PifDeviceReg;

/**
 * @class StPifDeviceRegOps
 * @brief Functions a bus driver gives to implement a PifDeviceReg. All of them are required.
 */
typedef struct StPifDeviceRegOps
{
	BOOL (*read)(PifDeviceReg *p_device, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size);	///< Reads size bytes from internal address iaddr of isize bytes, waiting until it is over.
	BOOL (*write)(PifDeviceReg *p_device, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size);	///< Writes size bytes to internal address iaddr of isize bytes, waiting until it is over.
	BOOL (*start_read)(PifDeviceReg *p_device, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size);	///< Starts a read and returns at once.
	PifDeviceRegState (*check_transfer)(PifDeviceReg *p_device);	///< Reports the state of the read started last.
	BOOL (*is_busy)(PifDeviceReg *p_device);	///< Tells whether the port is held by a transfer started with start_read.
} PifDeviceRegOps;

/**
 * @class StPifDeviceReg
 * @brief Device on a bus, as its clients see it.
 */
struct StPifDeviceReg
{
	// Public Member Variable
	uint16_t timeout;					///< Longest a transfer may take, in ms. 10 by default.
	uint16_t max_transfer_size;			///< Largest transfer the port takes, or 0 for any size.

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	void *_p_client;	///< Client context given to pifDeviceReg_Init()
	volatile PifDeviceRegState _state;		///< Of the transfer that holds the port for this device.

	// Private Member Variable
	const PifDeviceRegOps *__p_ops;
	uint32_t __start_time1ms;			///< When the transfer from pifDeviceReg_StartRead() started.
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up the device part of a bus device. A bus driver calls it when it adds a device.
 * @param p_owner Pointer to the device embedded in the bus device.
 * @param id Logical ID, or PIF_ID_AUTO.
 * @param p_client Client pointer kept for the port.
 * @param p_ops Functions of the bus driver. They must stay valid while the device is used.
 */
void pifDeviceReg_Init(PifDeviceReg *p_owner, PifId id, void *p_client, const PifDeviceRegOps *p_ops);

/**
 * @brief Reads a block from an internal address of the device, waiting until it is over. With
 *        max_transfer_size set, a longer block is read in pieces, each from its own internal address (iaddr
 *        plus the bytes already read), so the device has to step the address on by itself.
 *        Fails with pif_error set to E_INVALID_STATE, without touching the bus, while the port is held by a
 *        transfer from pifDeviceReg_StartRead() that pifDeviceReg_CheckTransfer() has not reported over yet. The same
 *        goes for every blocking function built on it.
 * @param p_owner Pointer to the device. NULL fails with E_INVALID_PARAM.
 * @param iaddr Internal address to read from.
 * @param isize Bytes of the internal address. 0 reads from where the device is.
 * @param p_data Buffer that receives the data.
 * @param size Bytes to read.
 * @return TRUE on success, otherwise FALSE with pif_error set.
 */
BOOL pifDeviceReg_Read(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size);

/**
 * @brief Writes a block to an internal address of the device, waiting until it is over, in pieces as
 *        pifDeviceReg_Read() reads. What a write of size 0 sends depends on the bus.
 * @param p_owner Pointer to the device. NULL fails with E_INVALID_PARAM.
 * @param iaddr Internal address to write to.
 * @param isize Bytes of the internal address. 0 writes from where the device is.
 * @param p_data Bytes to write.
 * @param size Bytes to write.
 * @return TRUE on success, otherwise FALSE with pif_error set.
 */
BOOL pifDeviceReg_Write(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size);

/**
 * @brief Reads one byte from an 8-bit register address.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param p_data Receives the value.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_ReadRegByte(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data);

/**
 * @brief Reads a 16-bit word, high byte first, from an 8-bit register address.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param p_data Receives the value.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_ReadRegWord(PifDeviceReg *p_owner, uint8_t reg, uint16_t *p_data);

/**
 * @brief Reads bytes starting at an 8-bit register address.
 * @param p_owner Pointer to the device.
 * @param reg Start register address.
 * @param p_data Buffer that receives the bytes.
 * @param size Bytes to read.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_ReadRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size);

/**
 * @brief Reads the bits of an 8-bit register under a mask.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param mask Bits of the field.
 * @param p_data Receives the register value with the other bits cleared.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_ReadRegBit8(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint8_t *p_data);

/**
 * @brief Reads the bits of a 16-bit register, high byte first, under a mask.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param mask Bits of the field.
 * @param p_data Receives the register value with the other bits cleared.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_ReadRegBit16(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint16_t *p_data);

/**
 * @brief Writes one byte to an 8-bit register address.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param data Value.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_WriteRegByte(PifDeviceReg *p_owner, uint8_t reg, uint8_t data);

/**
 * @brief Writes a 16-bit word, high byte first, to an 8-bit register address.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param data Value.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_WriteRegWord(PifDeviceReg *p_owner, uint8_t reg, uint16_t data);

/**
 * @brief Writes bytes starting at an 8-bit register address.
 * @param p_owner Pointer to the device.
 * @param reg Start register address.
 * @param p_data Bytes to write.
 * @param size Bytes to write.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_WriteRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size);

/**
 * @brief Changes the bits of an 8-bit register under a mask. The register is written only when they change.
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param mask Bits of the field.
 * @param data New bits, already in place under the mask. A value above mask fails with E_WRONG_DATA.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_WriteRegBit8(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint8_t data);

/**
 * @brief Changes the bits of a 16-bit register, high byte first, under a mask, as pifDeviceReg_WriteRegBit8().
 * @param p_owner Pointer to the device.
 * @param reg Register address.
 * @param mask Bits of the field.
 * @param data New bits, already in place under the mask.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDeviceReg_WriteRegBit16(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint16_t data);

/**
 * @brief Starts a read from an internal address of the device and returns at once. The port stays held for
 *        the device until pifDeviceReg_CheckTransfer() reports the read over, so ask it from later releases of the
 *        task instead of looping here. p_data has to stay valid until then. The read is not split, so size
 *        must not exceed max_transfer_size when it is set. On a port that cannot read in the background the
 *        read is already over when this returns, and pifDeviceReg_CheckTransfer() still reports it.
 *        Fails with pif_error set to E_INVALID_STATE while the port is held.
 * @param p_owner Pointer to the device.
 * @param iaddr Internal address to read from.
 * @param isize Bytes of the internal address. 0 reads from where the device is.
 * @param p_data Buffer that receives the data.
 * @param size Bytes to read.
 * @return TRUE if the read was started, otherwise FALSE with pif_error set.
 */
BOOL pifDeviceReg_StartRead(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size);

/**
 * @brief Starts a read of bytes from an 8-bit register address, as pifDeviceReg_StartRead().
 * @param p_owner Pointer to the device.
 * @param reg Start register address.
 * @param p_data Buffer that receives the bytes.
 * @param size Bytes to read.
 * @return TRUE if the read was started, otherwise FALSE.
 */
BOOL pifDeviceReg_StartReadRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size);

/**
 * @brief Reports how a read begun with pifDeviceReg_StartRead() stands, waiting for nothing. DRS_COMPLETE and
 *        DRS_ERROR are each reported once: they free the port. A read that runs past the device timeout is
 *        reported as DRS_ERROR with pif_error set to E_TIMEOUT.
 * @param p_owner Pointer to the device.
 * @return DRS_RUN while the read is running, DRS_COMPLETE or DRS_ERROR when it is over, and DRS_IDLE when
 *         nothing was started.
 */
PifDeviceRegState pifDeviceReg_CheckTransfer(PifDeviceReg *p_owner);

/**
 * @brief Tells whether a transfer from pifDeviceReg_StartRead(), for this device or another on its port, holds
 *        the port, so that any other transfer on the port would fail with E_INVALID_STATE now.
 * @param p_owner Pointer to the device.
 * @return TRUE while the port is held, otherwise FALSE.
 */
BOOL pifDeviceReg_IsBusy(PifDeviceReg *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DEVICE_REG_H
