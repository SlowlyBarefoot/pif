// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "communication/pif_comm.h"
#include "core/pif_obj_array.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


typedef enum EnPifI2cState
{
	IS_IDLE,
	IS_RUN,
	IS_COMPLETE,
	IS_ERROR
} PifI2cState;

typedef enum EnPifI2cReturn
{
	IR_WAIT,
	IR_COMPLETE,
	IR_ERROR
} PifI2cReturn;

#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifI2cCsFlag
{
	I2_CSF_OFF			= 0,

	I2_CSF_STATE_IDX	= 0,	// PifI2cState of the device using the port
	I2_CSF_ADDR_IDX		= 1,	// Address of the device whose transfer started last

	I2_CSF_STATE_BIT	= 1,
	I2_CSF_ADDR_BIT		= 2,
	I2_CSF_ALL_BIT		= 3,

	I2_CSF_COUNT		= 2
} PifI2cCsFlag;

#endif	// PIF_COLLECT_SIGNAL


struct StPifI2cDevice;
typedef struct StPifI2cDevice PifI2cDevice;

struct StPifI2cPort;
typedef struct StPifI2cPort PifI2cPort;

typedef PifI2cReturn (*PifActI2cRead)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef PifI2cReturn (*PifActI2cWrite)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef PifI2cReturn (*PifActI2cCheck)(PifI2cDevice *p_owner);
typedef void (*PifActI2cRecover)(PifI2cDevice *p_owner);

/**
 * @class StPifI2cDevice
 * @brief Describes an I2C slave device managed by a PifI2cPort instance.
 */
struct StPifI2cDevice
{
	// Public Member Variable
    uint8_t addr;
    uint16_t timeout;
    uint16_t max_transfer_size;

	// Read-only Member Variable
	volatile PifI2cState _state;
	PifId _id;
	PifI2cPort *_p_port;
	void *_p_client;

	// Private Member Variable
	uint32_t __start_time1ms;
};

/**
 * @class StPifI2cPort
 * @brief Owns an I2C bus context and manages registered slave devices.
 */
struct StPifI2cPort
{
	// Public Member Variable
	uint16_t error_count;

	// Public Action Function
	PifActI2cRead act_read;
	PifActI2cWrite act_write;
	// Optional. Polled while a transfer that act_read or act_write answered with IR_WAIT is
	// running, for a port that cannot call pifI2cPort_sigEndTransfer() from an interrupt. It
	// returns IR_WAIT until the transfer is over, then IR_COMPLETE or IR_ERROR.
	PifActI2cCheck act_check;
	// Optional. Called when a transfer that act_read or act_write answered with IR_WAIT runs past
	// the device timeout, before the port is given back, so the port can stop the transfer and
	// bring a stuck bus back (reset the peripheral, clock SDA free). It has to make sure the
	// stopped transfer never calls pifI2cPort_sigEndTransfer() afterwards: the signal does not say
	// which transfer it is for, so a late one would end the next transfer on the port instead.
	// A port without it has to keep such a late signal from coming the same way.
	PifActI2cRecover act_recover;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifObjArray __devices;
    volatile PifI2cDevice* __use_device;
#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[I2_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifI2cPort_Init
 * @brief Initializes an I2C port object and allocates its internal device list.
 * @param p_owner Pointer to the port object to initialize.
 * @param id Logical ID assigned to the port.
 * @param device_count Maximum number of devices the port can manage.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifI2cPort_Init(PifI2cPort *p_owner, PifId id, uint8_t device_count);

/**
 * @fn pifI2cPort_Clear
 * @brief Releases resources owned by the I2C port.
 * @param p_owner Pointer to the initialized port object.
 */
void pifI2cPort_Clear(PifI2cPort* p_owner);

/**
 * @fn pifI2cPort_AddDevice
 * @brief Registers a persistent I2C slave device on the port.
 * @param p_owner Pointer to the port that will own the device.
 * @param id Logical ID assigned to the new device.
 * @param addr 7-bit slave address.
 * @param p_client User-defined client pointer associated with the device.
 * @return Pointer to the created device, or `NULL` on failure.
 */
PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, PifId id, uint8_t addr, void *p_client);

/**
 * @fn pifI2cPort_RemoveDevice
 * @brief Unregisters a device from the port.
 * @param p_owner Pointer to the port that owns the device.
 * @param p_device Pointer to the device to remove. `NULL` does nothing.
 * @return `TRUE` if the device was removed, otherwise `FALSE` with pif_error set to
 *         E_INVALID_STATE while a transfer from pifI2cDevice_StartRead() still holds the port for
 *         it. The device stays on the port then.
 */
BOOL pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device);

/**
 * @fn pifI2cPort_TemporaryDevice
 * @brief Creates a temporary device descriptor for one-off transactions. Every call returns the
 *        same descriptor, set up anew, so it is only good until the next call.
 * @param p_owner Pointer to the port that performs the transaction.
 * @param addr 7-bit slave address of the temporary target.
 * @param p_client User-defined client pointer associated with the temporary device.
 * @return Pointer to the temporary device descriptor, or `NULL` with pif_error set to
 *         E_INVALID_STATE while a transfer from pifI2cDevice_StartRead() made with it still holds
 *         the port.
 */
PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr, void *p_client);

/**
 * @fn pifI2cDevice_Read
 * @brief Reads a block from an indexed location of an I2C device, waiting until it is over.
 *        With max_transfer_size set, a longer block is read in pieces, each from its own
 *        internal address (iaddr plus the bytes already read).
 *        Fails with pif_error set to E_INVALID_STATE, without touching the bus, while the port
 *        is held by a transfer from pifI2cDevice_StartRead() that pifI2cDevice_CheckTransfer()
 *        has not reported over yet. The same goes for every blocking function built on it.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param iaddr Internal address (register/memory offset) to read from.
 * @param isize Number of bytes used for the internal address.
 * @param p_data Output buffer that receives data.
 * @param size Number of bytes to read.
 * @return `TRUE` if the read operation completes successfully, otherwise `FALSE`.
 */
BOOL pifI2cDevice_Read(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_ReadRegByte
 * @brief Reads one byte from an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to read.
 * @param p_data Output pointer for the register value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_ReadRegByte(PifDevice* p_owner, uint8_t reg, uint8_t* p_data);

/**
 * @fn pifI2cDevice_ReadRegWord
 * @brief Reads a 16-bit word from an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to read.
 * @param p_data Output pointer for the 16-bit value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_ReadRegWord(PifDevice* p_owner, uint8_t reg, uint16_t* p_data);

/**
 * @fn pifI2cDevice_ReadRegBytes
 * @brief Reads multiple bytes starting at a register address.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Start register address.
 * @param p_data Output buffer for the received bytes.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_ReadRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_ReadRegBit8
 * @brief Reads masked bits from an 8-bit register.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to read.
 * @param mask Bit mask describing the field to extract.
 * @param p_data Output pointer for the extracted field value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_ReadRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t* p_data);

/**
 * @fn pifI2cDevice_ReadRegBit16
 * @brief Reads masked bits from a 16-bit register value.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to read.
 * @param mask Bit mask describing the field to extract.
 * @param p_data Output pointer for the extracted 16-bit field value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_ReadRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t* p_data);

/**
 * @fn pifI2cDevice_StartRead
 * @brief Starts a read from an indexed location of an I2C device and returns at once.
 *        The port stays taken by this device until pifI2cDevice_CheckTransfer() reports that the
 *        transfer is over, so ask it from later releases of the task instead of looping here.
 *        p_data has to stay valid until then. The read is not split by max_transfer_size, so
 *        size must not exceed it when it is set. Fails with pif_error set to E_INVALID_STATE
 *        while the port is held by another transfer.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param iaddr Internal address (register/memory offset) to read from.
 * @param isize Number of bytes used for the internal address.
 * @param p_data Output buffer that receives data.
 * @param size Number of bytes to read.
 * @return `TRUE` if the read was started, otherwise `FALSE`.
 */
BOOL pifI2cDevice_StartRead(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_StartReadRegBytes
 * @brief Starts a read of multiple bytes from a register address, as pifI2cDevice_StartRead().
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Start register address.
 * @param p_data Output buffer for the received bytes.
 * @param size Number of bytes to read.
 * @return `TRUE` if the read was started, otherwise `FALSE`.
 */
BOOL pifI2cDevice_StartReadRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_CheckTransfer
 * @brief Reports how a transfer begun with pifI2cDevice_StartRead() stands, waiting for nothing.
 *        IS_COMPLETE and IS_ERROR are each reported once: they free the port, and the device
 *        reads IS_IDLE again after that. A transfer that runs past the device timeout is
 *        reported as IS_ERROR with pif_error set to E_TIMEOUT, after act_recover has been called.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @return IS_RUN while the transfer is running, IS_COMPLETE or IS_ERROR when it is over, and
 *         IS_IDLE when nothing was started.
 */
PifI2cState pifI2cDevice_CheckTransfer(PifDevice* p_owner);

/**
 * @fn pifI2cDevice_Write
 * @brief Writes a block to an indexed location of an I2C device, waiting until it is over.
 *        With max_transfer_size set, a longer block is written in pieces, each to its own
 *        internal address (iaddr plus the bytes already written). With size 0 one transfer still
 *        goes out with no data: the internal address alone, a command for many devices, or only
 *        the slave address when isize is 0 too, which probes whether a device answers. act_write
 *        is then called with size 0, which the port has to support for this.
 *        Fails with pif_error set to E_INVALID_STATE, without touching the bus, while the port
 *        is held by a transfer from pifI2cDevice_StartRead() that pifI2cDevice_CheckTransfer()
 *        has not reported over yet. The same goes for every blocking function built on it.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param iaddr Internal address (register/memory offset) to write to.
 * @param isize Number of bytes used for the internal address.
 * @param p_data Input buffer containing bytes to write.
 * @param size Number of bytes to write.
 * @return `TRUE` if the write operation completes successfully, otherwise `FALSE`.
 */
BOOL pifI2cDevice_Write(PifDevice* p_owner,  uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_WriteRegByte
 * @brief Writes one byte to an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to write.
 * @param data Byte value to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_WriteRegByte(PifDevice* p_owner, uint8_t reg, uint8_t data);

/**
 * @fn pifI2cDevice_WriteRegWord
 * @brief Writes a 16-bit word to an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to write.
 * @param data 16-bit value to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_WriteRegWord(PifDevice* p_owner, uint8_t reg, uint16_t data);

/**
 * @fn pifI2cDevice_WriteRegBytes
 * @brief Writes multiple bytes starting at a register address.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Start register address.
 * @param p_data Input buffer containing bytes to write.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_WriteRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_WriteRegBit8
 * @brief Updates masked bits in an 8-bit register.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to update.
 * @param mask Bit mask describing the field to update.
 * @param data New field value (aligned to the mask definition).
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_WriteRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t data);

/**
 * @fn pifI2cDevice_WriteRegBit16
 * @brief Updates masked bits in a 16-bit register value.
 * @param p_owner Pointer to a `PifDevice` backed by an I2C device.
 * @param reg Register address to update.
 * @param mask Bit mask describing the field to update.
 * @param data New 16-bit field value (aligned to the mask definition).
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifI2cDevice_WriteRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t data);

/**
 * @fn pifI2cPort_sigEndTransfer
 * @brief Signals completion of an asynchronous I2C transfer. Ignored when no transfer is running,
 *        so only the first signal for a transfer counts. See act_recover about a signal that comes
 *        after a timeout.
 * @param p_owner Pointer to the port that initiated the transfer.
 * @param result Transfer result flag (`TRUE` for success, `FALSE` for error).
 */
void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result);

#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifI2cPort_SetCsFlag
 * @brief Adds the selected signals of the port to pifCollectSignal as channels. They follow the
 *        transfers of every device on the port.
 * @param p_owner Pointer to the port.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifI2cPort_SetCsFlag(PifI2cPort* p_owner, PifI2cCsFlag flag);

/**
 * @fn pifI2cPort_ResetCsFlag
 * @brief Removes the selected signals of the port from pifCollectSignal.
 * @param p_owner Pointer to the port.
 * @param flag Bit mask of the signals to remove.
 */
void pifI2cPort_ResetCsFlag(PifI2cPort* p_owner, PifI2cCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL


#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
