#ifndef PIF_SPI_H
#define PIF_SPI_H


#include "communication/pif_comm.h"
#include "core/pif_obj_array.h"


struct StPifSpiDevice;
typedef struct StPifSpiDevice PifSpiDevice;

struct StPifSpiPort;
typedef struct StPifSpiPort PifSpiPort;

typedef void (*PifActSpiTransfer)(PifSpiDevice *p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);

typedef BOOL (*PifActSpiRead)(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef BOOL (*PifActSpiWrite)(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

typedef BOOL (*PifActSpiIsBusy)(PifSpiDevice *p_owner);

/**
 * @class StPifSpiDevice
 * @brief Describes an SPI slave device managed by a PifSpiPort instance.
 */
struct StPifSpiDevice
{
	// Public Member Variable
    uint8_t timeout;
    uint16_t max_transfer_size;

	// Read-only Member Variable
	PifId _id;
	PifSpiPort *_p_port;
	void *_p_client;
};

/**
 * @class StPifSpiPort
 * @brief Owns an SPI bus context and manages registered slave devices.
 */
typedef struct StPifSpiPort
{
	// Public Member Variable
	uint16_t error_count;

	// Public Action Function
	PifActSpiTransfer act_transfer;
	PifActSpiRead act_read;
	PifActSpiWrite act_write;
	PifActSpiIsBusy act_is_busy;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifObjArray __devices;
} PifSpiPort;


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
 * @param p_device Pointer to the device to remove.
 */
void pifSpiPort_RemoveDevice(PifSpiPort* p_owner, PifSpiDevice* p_device);

/**
 * @fn pifSpiPort_TemporaryDevice
 * @brief Creates a temporary device descriptor for one-off SPI transactions.
 * @param p_owner Pointer to the port that performs the transaction.
 * @param p_client User-defined client pointer associated with the temporary device.
 * @return Pointer to the temporary device descriptor, or `NULL` if unavailable.
 */
PifSpiDevice* pifSpiPort_TemporaryDevice(PifSpiPort* p_owner, void *p_client);

/**
 * @fn pifSpiDevice_Transfer
 * @brief Performs a full-duplex SPI transfer.
 * @param p_owner Pointer to the SPI device descriptor.
 * @param p_write Buffer containing data to transmit (can be `NULL` for dummy writes).
 * @param p_read Buffer receiving incoming data (can be `NULL` if readback is not needed).
 * @param size Number of bytes to transfer.
 * @return `TRUE` if the transfer request is accepted and completed, otherwise `FALSE`.
 */
BOOL pifSpiDevice_Transfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);

/**
 * @fn pifSpiDevice_Read
 * @brief Reads a block from an indexed location of an SPI device.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param iaddr Internal address (register/memory offset) to read from.
 * @param isize Number of bytes used for the internal address.
 * @param p_data Output buffer that receives data.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_Read(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_ReadRegByte
 * @brief Reads one byte from an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to read.
 * @param p_data Output pointer for the register value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_ReadRegByte(PifDevice* p_owner, uint8_t reg, uint8_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegWord
 * @brief Reads a 16-bit word from an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to read.
 * @param p_data Output pointer for the 16-bit value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_ReadRegWord(PifDevice* p_owner, uint8_t reg, uint16_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegBytes
 * @brief Reads multiple bytes starting at a register address.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Start register address.
 * @param p_data Output buffer for the received bytes.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_ReadRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_ReadRegBit8
 * @brief Reads masked bits from an 8-bit register.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to read.
 * @param mask Bit mask describing the field to extract.
 * @param p_data Output pointer for the extracted field value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_ReadRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegBit16
 * @brief Reads masked bits from a 16-bit register value.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to read.
 * @param mask Bit mask describing the field to extract.
 * @param p_data Output pointer for the extracted 16-bit field value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_ReadRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t* p_data);

/**
 * @fn pifSpiDevice_Write
 * @brief Writes a block to an indexed location of an SPI device.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param iaddr Internal address (register/memory offset) to write to.
 * @param isize Number of bytes used for the internal address.
 * @param p_data Input buffer containing bytes to write.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_Write(PifDevice* p_owner,  uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_WriteRegByte
 * @brief Writes one byte to an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to write.
 * @param data Byte value to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_WriteRegByte(PifDevice* p_owner, uint8_t reg, uint8_t data);

/**
 * @fn pifSpiDevice_WriteRegWord
 * @brief Writes a 16-bit word to an 8-bit register address.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to write.
 * @param data 16-bit value to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_WriteRegWord(PifDevice* p_owner, uint8_t reg, uint16_t data);

/**
 * @fn pifSpiDevice_WriteRegBytes
 * @brief Writes multiple bytes starting at a register address.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Start register address.
 * @param p_data Input buffer containing bytes to write.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_WriteRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_WriteRegBit8
 * @brief Updates masked bits in an 8-bit register.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to update.
 * @param mask Bit mask describing the field to update.
 * @param data New field value (aligned to the mask definition).
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_WriteRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t data);

/**
 * @fn pifSpiDevice_WriteRegBit16
 * @brief Updates masked bits in a 16-bit register value.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param reg Register address to update.
 * @param mask Bit mask describing the field to update.
 * @param data New 16-bit field value (aligned to the mask definition).
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifSpiDevice_WriteRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t data);

/**
 * @fn pifSpiDevice_IsBusy
 * @brief Checks whether the underlying SPI bus/device is busy.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @return `TRUE` if a transfer is in progress, otherwise `FALSE`.
 */
BOOL pifSpiDevice_IsBusy(PifDevice* p_owner);

/**
 * @fn pifSpiDevice_Wait
 * @brief Waits until the SPI device finishes an outstanding transfer.
 * @param p_owner Pointer to a `PifDevice` backed by an SPI device.
 * @param timeout1ms
 * @return `TRUE` if the device becomes ready before timeout, otherwise `FALSE`.
 */
BOOL pifSpiDevice_Wait(PifDevice* p_owner, uint16_t timeout1ms);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SPI_H
