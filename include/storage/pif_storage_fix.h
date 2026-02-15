#ifndef PIF_STORAGE_FIX_H
#define PIF_STORAGE_FIX_H


#include "storage/pif_storage.h"


/**
 * @class StPifStorageFixDataInfo
 * @brief Metadata for a fixed-slot storage record.
 */
typedef struct StPifStorageFixDataInfo
{
	uint16_t id;
} PifStorageFixDataInfo;

/**
 * @class StPifStorageFix
 * @brief Fixed-size storage manager built on top of the generic storage interface.
 */
typedef struct StPifStorageFix
{
	// The parent variable must be at the beginning of this structure.
	PifStorage parent;

	// Public Member Variable

	// Read-only Member Variable

	// Private Member Variable
    uint16_t __data_info_count;
    uint32_t __sector_size;
	PifStorageFixDataInfo* __p_data_info;

	// Private Action Function
} PifStorageFix;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStorageFix_Init
 * @brief Initializes a fixed storage manager instance.
 * @param p_owner Pointer to the fixed storage instance.
 * @param id Object id, or `PIF_ID_AUTO` to allocate automatically.
 * @return `TRUE` on successful initialization, otherwise `FALSE`.
 */
BOOL pifStorageFix_Init(PifStorageFix* p_owner, PifId id);

/**
 * @fn pifStorageFix_Clear
 * @brief Releases dynamic resources allocated by the fixed storage manager.
 * @param p_owner Pointer to the fixed storage instance.
 */
void pifStorageFix_Clear(PifStorageFix* p_owner);

/**
 * @fn pifStorageFix_AttachActStorage
 * @brief Attaches low-level media access callbacks to the fixed storage manager.
 * @param p_owner Pointer to the fixed storage instance.
 * @param act_read Callback for media read operations.
 * @param act_write Callback for media write operations.
 * @return `TRUE` if callbacks are attached, otherwise `FALSE`.
 */
BOOL pifStorageFix_AttachActStorage(PifStorageFix* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);

/**
 * @fn pifStorageFix_AttachI2c
 * @brief Attaches an I2C-backed media device to the fixed storage manager.
 * @param p_owner Pointer to the fixed storage instance.
 * @param p_port I2C port used for communication.
 * @param addr Device address.
 * @param p_client Optional client pointer for I2C device registration.
 * @param i_addr_size Width of internal media address bytes.
 * @param write_delay_ms Delay after write transactions in milliseconds.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageFix_AttachI2c(PifStorageFix* p_owner, PifI2cPort* p_port, uint8_t addr, void *p_client, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifStorageFix_SetMedia
 * @brief Configures fixed media geometry and allocates record metadata.
 * @param p_owner Pointer to the fixed storage instance.
 * @param sector_size Slot size in bytes.
 * @param storage_volume Total storage capacity in bytes.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageFix_SetMedia(PifStorageFix* p_owner, uint32_t sector_size, uint32_t storage_volume);

/**
 * @fn pifStorageFix_IsFormat
 * @brief Reports whether fixed storage is considered formatted.
 * @param p_parent Pointer to base storage interface.
 * @return `TRUE` for fixed storage implementation.
 */
BOOL pifStorageFix_IsFormat(PifStorage* p_parent);

/**
 * @fn pifStorageFix_Format
 * @brief Performs fixed storage format routine.
 * @param p_parent Pointer to base storage interface.
 * @return `TRUE` for fixed storage implementation.
 */
BOOL pifStorageFix_Format(PifStorage* p_parent);

/**
 * @fn pifStorageFix_Create
 * @brief Creates or resolves a fixed storage entry by id.
 * @param p_parent Pointer to base storage interface.
 * @param id Entry id.
 * @param size Requested size (unused in fixed storage mode).
 * @return Data info handle on success, otherwise `NULL`.
 */
PifStorageDataInfoP pifStorageFix_Create(PifStorage* p_parent, uint16_t id, uint16_t size);

/**
 * @fn pifStorageFix_Delete
 * @brief Deletes a fixed storage entry metadata by id.
 * @param p_parent Pointer to base storage interface.
 * @param id Entry id.
 * @return `TRUE` for fixed storage implementation.
 */
BOOL pifStorageFix_Delete(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageFix_Open
 * @brief Opens a fixed storage entry by id.
 * @param p_parent Pointer to base storage interface.
 * @param id Entry id.
 * @return Data info handle on success, otherwise `NULL`.
 */
PifStorageDataInfoP pifStorageFix_Open(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageFix_Read
 * @brief Reads bytes from a fixed storage entry.
 * @param p_parent Pointer to base storage interface.
 * @param p_dst Destination buffer.
 * @param p_src Source entry metadata handle.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageFix_Read(PifStorage* p_parent, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);

/**
 * @fn pifStorageFix_Write
 * @brief Writes bytes to a fixed storage entry.
 * @param p_parent Pointer to base storage interface.
 * @param p_dst Destination entry metadata handle.
 * @param p_src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageFix_Write(PifStorage* p_parent, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_FIX_H
