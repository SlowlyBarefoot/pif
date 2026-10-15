// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "communication/pif_device_reg.h"
#include "core/pif_obj_array.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_I2C)
	#define COLLECT_SIGNAL_I2C
	#include "core/pif_collect_signal.h"
#endif


typedef enum EnPifI2cReturn
{
	IR_WAIT,
	IR_COMPLETE,
	IR_ERROR
} PifI2cReturn;

#ifdef COLLECT_SIGNAL_I2C

typedef enum EnPifI2cCsFlag
{
	I2_CSF_OFF			= 0,

	I2_CSF_STATE_IDX	= 0,	// PifDeviceRegState of the device using the port
	I2_CSF_ADDR_IDX		= 1,	// Address of the device whose transfer started last

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

typedef PifI2cReturn (*PifActI2cRead)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef PifI2cReturn (*PifActI2cWrite)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef PifI2cReturn (*PifActI2cCheck)(PifI2cDevice *p_owner);
typedef void (*PifActI2cRecover)(PifI2cDevice *p_owner);

/**
 * @class StPifI2cDevice
 * @brief Describes an I2C slave device managed by a PifI2cPort instance. Its clients use &device->parent
 *        with the pifDeviceReg_ functions.
 */
struct StPifI2cDevice
{
	PifDeviceReg parent;

	// Public Member Variable
	uint8_t addr;

	// Read-only Member Variable
	PifI2cPort *_p_port;
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
#ifdef COLLECT_SIGNAL_I2C
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
 *         E_INVALID_STATE while a transfer from pifDeviceReg_StartRead() still holds the port for it.
 *         The device stays on the port then.
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
 *         E_INVALID_STATE while a transfer from pifDeviceReg_StartRead() made with it still holds the
 *         port.
 */
PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr, void *p_client);

/**
 * @fn pifI2cPort_sigEndTransfer
 * @brief Signals completion of an asynchronous I2C transfer. Ignored when no transfer is running,
 *        so only the first signal for a transfer counts. See act_recover about a signal that comes
 *        after a timeout.
 * @param p_owner Pointer to the port that initiated the transfer.
 * @param result Transfer result flag (`TRUE` for success, `FALSE` for error).
 */
void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result);

#ifdef COLLECT_SIGNAL_I2C

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

#endif	// COLLECT_SIGNAL_I2C


#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
