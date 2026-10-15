// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SPI_H
#define PIF_SPI_H


#include "communication/pif_device_reg.h"
#include "core/pif_obj_array.h"


struct StPifSpiDevice;
typedef struct StPifSpiDevice PifSpiDevice;

struct StPifSpiPort;
typedef struct StPifSpiPort PifSpiPort;

typedef void (*PifActSpiTransfer)(PifSpiDevice *p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);
typedef BOOL (*PifActSpiStartTransfer)(PifSpiDevice *p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);

typedef BOOL (*PifActSpiRead)(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef BOOL (*PifActSpiWrite)(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

typedef BOOL (*PifActSpiIsBusy)(PifSpiDevice *p_owner);

/**
 * @class StPifSpiDevice
 * @brief Describes an SPI slave device managed by a PifSpiPort instance. Its clients use &device->parent
 *        with the pifDeviceReg_ functions.
 */
struct StPifSpiDevice
{
	PifDeviceReg parent;

	// Read-only Member Variable
	PifSpiPort *_p_port;
};

/**
 * @class StPifSpiPort
 * @brief Owns an SPI bus context and manages registered slave devices.
 * @details A transfer that runs in the background holds the port, as on pif_i2c: until
 *          pifDeviceReg_CheckTransfer() reports it over, every other transfer on the port fails with
 *          E_INVALID_STATE without reaching the port.
 */
struct StPifSpiPort
{
	// Public Member Variable
	uint16_t error_count;

	// Public Action Function
	// Makes a full-duplex transfer under one chip select and returns when it is over.
	PifActSpiTransfer act_transfer;
	// Optional. Starts a transfer like act_transfer and returns without waiting for it, for a
	// port that can run it in the background (DMA, interrupts). It returns FALSE when the transfer
	// could not be started. The port ends the transfer with pifSpiPort_sigEndTransfer(), from the
	// interrupt that ended it if it has one, or reports it with act_is_busy.
	PifActSpiStartTransfer act_start_transfer;
	// Send the internal address, then read or write the data, under one chip select, and return
	// when it is over. A block longer than max_transfer_size comes as several calls, each with its
	// own address.
	PifActSpiRead act_read;
	PifActSpiWrite act_write;
	// Optional. Starts a read like act_read and returns without waiting for it, ended as a transfer
	// from act_start_transfer is. Without it pifDeviceReg_StartRead() reads with act_read.
	PifActSpiRead act_start_read;
	// Optional. Polled while a transfer from act_start_transfer or act_start_read is running, for a
	// port that cannot call pifSpiPort_sigEndTransfer(). It returns TRUE until the transfer is over.
	PifActSpiIsBusy act_is_busy;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifObjArray __devices;
    volatile PifSpiDevice* __use_device;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSpiPort_Init
 * @brief Initializes an SPI port object and allocates its internal device list.
 * @param p_owner Pointer to the port object to initialize.
 * @param id Logical ID assigned to the port.
 * @param device_count Maximum number of devices the port can manage.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifSpiPort_Init(PifSpiPort* p_owner, PifId id, uint8_t device_count);

/**
 * @fn pifSpiPort_Clear
 * @brief Releases resources owned by the SPI port.
 * @param p_owner Pointer to the initialized port object.
 */
void pifSpiPort_Clear(PifSpiPort* p_owner);

/**
 * @fn pifSpiPort_AddDevice
 * @brief Registers a persistent SPI slave device on the port.
 * @param p_owner Pointer to the port that will own the device.
 * @param id Logical ID assigned to the new device.
 * @param p_client User-defined client pointer associated with the device.
 * @return Pointer to the created device, or `NULL` on failure.
 */
PifSpiDevice* pifSpiPort_AddDevice(PifSpiPort* p_owner, PifId id, void *p_client);

/**
 * @fn pifSpiPort_RemoveDevice
 * @brief Unregisters a device from the port.
 * @param p_owner Pointer to the port that owns the device.
 * @param p_device Pointer to the device to remove. `NULL` does nothing.
 * @return `TRUE` if the device was removed, otherwise `FALSE` with pif_error set to
 *         E_INVALID_STATE while a background transfer still holds the port for it.
 */
BOOL pifSpiPort_RemoveDevice(PifSpiPort* p_owner, PifSpiDevice* p_device);

/**
 * @fn pifSpiPort_TemporaryDevice
 * @brief Creates a temporary device descriptor for one-off SPI transactions. Every call returns
 *        the same descriptor, set up anew, so it is only good until the next call.
 * @param p_owner Pointer to the port that performs the transaction.
 * @param p_client User-defined client pointer associated with the temporary device.
 * @return Pointer to the temporary device descriptor, or `NULL` with pif_error set to
 *         E_INVALID_STATE while a background transfer made with it still holds the port.
 */
PifSpiDevice* pifSpiPort_TemporaryDevice(PifSpiPort* p_owner, void *p_client);

/**
 * @fn pifSpiDevice_Transfer
 * @brief Performs a full-duplex SPI transfer with act_transfer, under one chip select, so it is
 *        not split by max_transfer_size and size must not exceed it when it is set.
 * @param p_owner Pointer to the SPI device descriptor.
 * @param p_write Buffer containing data to transmit (can be `NULL` for dummy writes).
 * @param p_read Buffer receiving incoming data (can be `NULL` if readback is not needed).
 * @param size Number of bytes to transfer.
 * @return `TRUE` once act_transfer has made the transfer, otherwise `FALSE` with pif_error set
 *         to `E_INVALID_PARAM` for a bad request or a port without act_transfer, or
 *         `E_INVALID_STATE` while a background transfer holds the port. act_transfer reports no
 *         failure, so `TRUE` does not say the device answered.
 */
BOOL pifSpiDevice_Transfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);

/**
 * @fn pifSpiDevice_StartTransfer
 * @brief Starts a full-duplex SPI transfer and returns without waiting for it to finish. The
 *        port stays held until pifDeviceReg_CheckTransfer() reports the transfer over, so ask it
 *        from later releases of the task instead of looping here. p_write and p_read have to stay
 *        valid until then. The transfer is not split by max_transfer_size, so size must not
 *        exceed it when it is set. On a port without act_start_transfer the transfer is made with
 *        act_transfer and is already over when this returns.
 * @param p_owner Pointer to the SPI device descriptor.
 * @param p_write Buffer containing data to transmit (can be `NULL` for dummy writes).
 * @param p_read Buffer receiving incoming data (can be `NULL` if readback is not needed).
 * @param size Number of bytes to transfer.
 * @return `TRUE` if the transfer was started, otherwise `FALSE` with pif_error set to
 *         `E_INVALID_PARAM` for a bad request, `E_INVALID_STATE` while the port is held, or
 *         `E_TRANSFER_FAILED` when the port could not start it.
 */
BOOL pifSpiDevice_StartTransfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);

/**
 * @fn pifSpiPort_sigEndTransfer
 * @brief Signals that a background transfer on the port is over. Called by the port, from
 *        interrupt context if that is where it learns of it. Ignored when no transfer is running,
 *        so only the first signal for a transfer counts. A transfer that ran past the device
 *        timeout has been given up by then, so the port has to keep its late signal from ending
 *        the next transfer.
 * @param p_owner Pointer to the port.
 * @param result TRUE when the transfer succeeded, FALSE for an error.
 */
void pifSpiPort_sigEndTransfer(PifSpiPort* p_owner, BOOL result);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SPI_H
