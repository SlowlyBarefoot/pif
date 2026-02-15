#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "communication/pif_comm.h"
#include "core/pif_obj_array.h"


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


struct StPifI2cDevice;
typedef struct StPifI2cDevice PifI2cDevice;

struct StPifI2cPort;
typedef struct StPifI2cPort PifI2cPort;

typedef PifI2cReturn (*PifActI2cRead)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);
typedef PifI2cReturn (*PifActI2cWrite)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

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

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifObjArray __devices;
    volatile PifI2cDevice* __use_device;
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
 * @param p_device Pointer to the device to remove.
 */
void pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device);

/**
 * @fn pifI2cPort_TemporaryDevice
 * @brief Creates a temporary device descriptor for one-off transactions.
 * @param p_owner Pointer to the port that performs the transaction.
 * @param addr 7-bit slave address of the temporary target.
 * @param p_client User-defined client pointer associated with the temporary device.
 * @return Pointer to the temporary device descriptor, or `NULL` if unavailable.
 */
PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr, void *p_client);

#ifndef PIF_NO_LOG

/**
 * @fn pifI2cPort_ScanAddress
 * @brief Scans bus addresses and logs discovered devices.
 * @param p_owner Pointer to the port to scan.
 */
void pifI2cPort_ScanAddress(PifI2cPort* p_owner);

#endif

/**
 * @fn pifI2cDevice_Read
 * @brief Reads a block from an indexed location of an I2C device.
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
 * @fn pifI2cDevice_Write
 * @brief Writes a block to an indexed location of an I2C device.
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
 * @brief Signals completion of an asynchronous I2C transfer.
 * @param p_owner Pointer to the port that initiated the transfer.
 * @param result Transfer result flag (`TRUE` for success, `FALSE` for error).
 */
void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result);

#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
