#ifndef PIF_STORAGE_H
#define PIF_STORAGE_H


#include "communication/pif_i2c.h"


typedef enum EnPifStorageI2cIAddrSize
{
    SIC_I_ADDR_SIZE_1	= 1,
    SIC_I_ADDR_SIZE_2	= 2
} PifStorageI2cIAddrSize;


typedef void* PifStorageDataInfoP;

struct StPifStorage;
typedef struct StPifStorage PifStorage;

typedef BOOL (*PifActStorageRead)(PifStorage* p_owner, uint8_t* dst, uint32_t src, size_t size);
typedef BOOL (*PifActStorageWrite)(PifStorage* p_owner, uint32_t src, uint8_t* dst, size_t size);

typedef BOOL (*PifStorage_IsFormat)(PifStorage* p_owner);
typedef BOOL (*PifStorage_Format)(PifStorage* p_owner);
typedef PifStorageDataInfoP (*PifStorage_Create)(PifStorage* p_owner, uint16_t id, uint16_t size);
typedef BOOL (*PifStorage_Delete)(PifStorage* p_owner, uint16_t id);
typedef PifStorageDataInfoP (*PifStorage_Open)(PifStorage* p_owner, uint16_t id);
typedef BOOL (*PifStorage_Read)(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);
typedef BOOL (*PifStorage_Write)(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);


/**
 * @class StPifStorage
 * @brief Base storage interface that abstracts fixed and variable storage backends.
 */
struct StPifStorage
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;
	PifI2cDevice* _p_i2c;
	uint32_t _storage_volume;

	// Private Member Variable
	uint8_t __addr;
	uint8_t __write_delay_ms;

	// Private Function
	PifStorage_IsFormat __fn_is_format;
	PifStorage_Format __fn_format;
	PifStorage_Create __fn_create;
	PifStorage_Delete __fn_delete;
	PifStorage_Open __fn_open;
	PifStorage_Read __fn_read;
	PifStorage_Write __fn_write;

	// Private Action Function
	PifActStorageRead __act_read;
	PifActStorageWrite __act_write;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStorage_AttachActStorage
 * @brief Attaches low-level read and write callbacks to a storage instance.
 * @param p_owner Pointer to the storage instance.
 * @param act_read Callback used to read bytes from the underlying media.
 * @param act_write Callback used to write bytes to the underlying media.
 * @return `TRUE` if callbacks are valid and attached, otherwise `FALSE`.
 */
BOOL pifStorage_AttachActStorage(PifStorage* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);

/**
 * @fn pifStorage_AttachI2c
 * @brief Attaches an I2C device and configures internal address handling callbacks.
 * @param p_owner Pointer to the storage instance.
 * @param p_port Pointer to the I2C port used to access the storage device.
 * @param addr Device I2C address or address base.
 * @param p_client Optional client pointer passed to the I2C device registration.
 * @param i_addr_size Internal storage address width in bytes.
 * @param write_delay_ms Delay in milliseconds applied after each write operation.
 * @return `TRUE` if the I2C device is attached successfully, otherwise `FALSE`.
 */
BOOL pifStorage_AttachI2c(PifStorage* p_owner, PifI2cPort* p_port, uint8_t addr, void *p_client, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifStorage_DetachI2c
 * @brief Detaches the currently attached I2C device and clears access callbacks.
 * @param p_owner Pointer to the storage instance.
 */
void pifStorage_DetachI2c(PifStorage* p_owner);

/**
 * @fn pifStorage_IsFormat
 * @brief Checks whether the storage media is currently formatted.
 * @param p_owner Pointer to the storage instance.
 * @return `TRUE` when media format metadata is valid, otherwise `FALSE`.
 */
BOOL pifStorage_IsFormat(PifStorage* p_owner);

/**
 * @fn pifStorage_Format
 * @brief Formats the storage media and initializes allocation metadata.
 * @param p_owner Pointer to the storage instance.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorage_Format(PifStorage* p_owner);

/**
 * @fn pifStorage_Create
 * @brief Creates a data entry in storage with the requested id and size.
 * @param p_owner Pointer to the storage instance.
 * @param id Logical data identifier.
 * @param size Requested payload size in bytes.
 * @return Opaque data info pointer on success, or `NULL` on failure.
 */
PifStorageDataInfoP pifStorage_Create(PifStorage* p_owner, uint16_t id, uint16_t size);

/**
 * @fn pifStorage_Delete
 * @brief Deletes a data entry identified by id from storage metadata.
 * @param p_owner Pointer to the storage instance.
 * @param id Logical data identifier to remove.
 * @return `TRUE` if the entry is removed, otherwise `FALSE`.
 */
BOOL pifStorage_Delete(PifStorage* p_owner, uint16_t id);

/**
 * @fn pifStorage_Open
 * @brief Opens an existing data entry and returns its metadata handle.
 * @param p_owner Pointer to the storage instance.
 * @param id Logical data identifier to open.
 * @return Opaque data info pointer on success, or `NULL` if not found.
 */
PifStorageDataInfoP pifStorage_Open(PifStorage* p_owner, uint16_t id);

/**
 * @fn pifStorage_Read
 * @brief Reads bytes from a storage entry into a destination buffer.
 * @param p_owner Pointer to the storage instance.
 * @param p_dst Destination buffer.
 * @param p_src Opaque metadata handle of the source storage entry.
 * @param size Number of bytes to read.
 * @return `TRUE` if read succeeds, otherwise `FALSE`.
 */
BOOL pifStorage_Read(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);

/**
 * @fn pifStorage_Write
 * @brief Writes bytes from a source buffer into a storage entry.
 * @param p_owner Pointer to the storage instance.
 * @param p_dst Opaque metadata handle of the destination storage entry.
 * @param p_src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` if write succeeds, otherwise `FALSE`.
 */
BOOL pifStorage_Write(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_H
