// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_i2c.h
 * @brief I2C port with the devices on it, as PifDeviceReg devices.
 *
 * The platform gives a PifI2cPort the functions that move bytes on its bus. Each slave on the bus is a
 * PifI2cDevice, and the drivers of the slaves use it through &device->parent with the pifDeviceReg_ functions. A
 * transfer can run in the background and end with pifI2cPort_sigEndTransfer() or act_check.
 */
#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "communication/pif_device_reg.h"
#include "core/pif_obj_array.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_I2C)
	#define COLLECT_SIGNAL_I2C	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_NO_I2C is not
	#include "core/pif_collect_signal.h"
#endif


/**
 * @brief Answer of the port functions to a transfer.
 */
typedef enum EnPifI2cReturn
{
	IR_WAIT,	///< Started. It ends later with pifI2cPort_sigEndTransfer() or act_check.
	IR_COMPLETE,	///< Done
	IR_ERROR	///< Failed
} PifI2cReturn;

#ifdef COLLECT_SIGNAL_I2C

/**
 * @brief Signals of a port for pifI2cPort_SetCsFlag() and pifI2cPort_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifI2cCsFlag
{
	I2_CSF_OFF			= 0,

	I2_CSF_STATE_IDX	= 0,	///< PifDeviceRegState of the device using the port
	I2_CSF_ADDR_IDX		= 1,	///< Address of the device whose transfer started last

	I2_CSF_STATE_BIT	= 1,
	I2_CSF_ADDR_BIT		= 2,
	I2_CSF_ALL_BIT		= 3,

	I2_CSF_COUNT		= 2
} PifI2cCsFlag;

#endif	// COLLECT_SIGNAL_I2C


struct StPifI2cDevice;
typedef struct StPifI2cDevice PifI2cDevice;

struct StPifI2cPort;
typedef struct StPifI2cPort PifI2cPort;

/**
 * @brief Reads size bytes from internal address iaddr of isize bytes (0 for none) of a device.
 */
typedef PifI2cReturn (*PifActI2cRead)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
/**
 * @brief Writes size bytes to internal address iaddr of isize bytes (0 for none) of a device.
 */
typedef PifI2cReturn (*PifActI2cWrite)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
/**
 * @brief Polls a transfer that act_read or act_write answered with IR_WAIT.
 */
typedef PifI2cReturn (*PifActI2cCheck)(PifI2cDevice *p_owner);
/**
 * @brief Stops a transfer that ran past the device timeout and frees a stuck bus.
 */
typedef void (*PifActI2cRecover)(PifI2cDevice *p_owner);

/**
 * @class StPifI2cDevice
 * @brief Describes an I2C slave device managed by a PifI2cPort instance. Its clients use &device->parent
 *        with the pifDeviceReg_ functions.
 */
struct StPifI2cDevice
{
	PifDeviceReg parent;	///< Register access of the device. Must stay the first member.

	// Public Member Variable
	uint8_t addr;	///< 7 bit slave address

	// Read-only Member Variable
	PifI2cPort *_p_port;	///< Port the device is on
};

/**
 * @class StPifI2cPort
 * @brief Owns an I2C bus context and manages registered slave devices.
 */
struct StPifI2cPort
{
	// Public Member Variable
	uint16_t error_count;	///< Transfers that failed. The port counts, the application may clear it.

	// Public Action Function
	PifActI2cRead act_read;	///< Reads from a device. Required.
	PifActI2cWrite act_write;	///< Writes to a device. Required.
	/// Optional. Polled while a transfer that act_read or act_write answered with IR_WAIT is
	/// running, for a port that cannot call pifI2cPort_sigEndTransfer() from an interrupt. It
	/// returns IR_WAIT until the transfer is over, then IR_COMPLETE or IR_ERROR.
	PifActI2cCheck act_check;
	/// Optional. Called when a transfer that act_read or act_write answered with IR_WAIT runs past
	/// the device timeout, before the port is given back, so the port can stop the transfer and
	/// bring a stuck bus back (reset the peripheral, clock SDA free). It has to make sure the
	/// stopped transfer never calls pifI2cPort_sigEndTransfer() afterwards: the signal does not say
	/// which transfer it is for, so a late one would end the next transfer on the port instead.
	/// A port without it has to keep such a late signal from coming the same way.
	PifActI2cRecover act_recover;

	// Read-only Member Variable
	PifId _id;	///< Instance ID

	// Private Member Variable
    PifObjArray __devices;
    volatile PifI2cDevice* __use_device;
#ifdef COLLECT_SIGNAL_I2C
	PifCollectSignalChannel __cs[I2_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an I2C port object and allocates its internal device list.
 * @param p_owner Pointer to the port object to initialize.
 * @param id Logical ID assigned to the port.
 * @param device_count Maximum number of devices the port can manage.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifI2cPort_Init(PifI2cPort *p_owner, PifId id, uint8_t device_count);

/**
 * @brief Releases resources owned by the I2C port.
 * @param p_owner Pointer to the initialized port object.
 */
void pifI2cPort_Clear(PifI2cPort* p_owner);

/**
 * @brief Registers a persistent I2C slave device on the port.
 * @param p_owner Pointer to the port that will own the device.
 * @param id Logical ID assigned to the new device.
 * @param addr 7-bit slave address.
 * @param p_client User-defined client pointer associated with the device.
 * @return Pointer to the created device, or `NULL` on failure.
 */
PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, PifId id, uint8_t addr, void *p_client);

/**
 * @brief Unregisters a device from the port.
 * @param p_owner Pointer to the port that owns the device.
 * @param p_device Pointer to the device to remove. `NULL` does nothing.
 * @return `TRUE` if the device was removed, otherwise `FALSE` with pif_error set to
 *         E_INVALID_STATE while a transfer from pifDeviceReg_StartRead() still holds the port for it.
 *         The device stays on the port then.
 */
BOOL pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device);

/**
 * @brief Creates a temporary device descriptor for one-off transactions. Every call returns the
 *        same descriptor, set up anew, so it is only good until the next call.
 * @param p_owner Pointer to the port that performs the transaction.
 * @param addr 7-bit slave address of the temporary target.
 * @param p_client User-defined client pointer associated with the temporary device.
 * @return Pointer to the temporary device descriptor, or `NULL` with pif_error set to
 *         E_INVALID_STATE while a transfer from pifDeviceReg_StartRead() made with it still holds the
 *         port.
 */
PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr, void *p_client);

/**
 * @brief Signals completion of an asynchronous I2C transfer. Ignored when no transfer is running,
 *        so only the first signal for a transfer counts. See act_recover about a signal that comes
 *        after a timeout.
 * @param p_owner Pointer to the port that initiated the transfer.
 * @param result Transfer result flag (`TRUE` for success, `FALSE` for error).
 */
void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result);

#ifdef COLLECT_SIGNAL_I2C

/**
 * @brief Adds the selected signals of the port to pifCollectSignal as channels. They follow the
 *        transfers of every device on the port.
 * @param p_owner Pointer to the port.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifI2cPort_SetCsFlag(PifI2cPort* p_owner, PifI2cCsFlag flag);

/**
 * @brief Removes the selected signals of the port from pifCollectSignal.
 * @param p_owner Pointer to the port.
 * @param flag Bit mask of the signals to remove.
 */
void pifI2cPort_ResetCsFlag(PifI2cPort* p_owner, PifI2cCsFlag flag);

#endif	// COLLECT_SIGNAL_I2C


#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
