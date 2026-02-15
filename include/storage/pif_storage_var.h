#ifndef PIF_STORAGE_VAR_H
#define PIF_STORAGE_VAR_H


#include "storage/pif_storage.h"


/**
 * @class StPifStorageVarDataInfo
 * @brief Metadata node that describes one variable-length data allocation.
 */
typedef struct StPifStorageVarDataInfo
{
	uint16_t id;
	uint16_t size;
	uint16_t first_sector;

	uint16_t next_node;
	uint16_t prev_node;
	uint16_t crc_16;
} PifStorageVarDataInfo;

/**
 * @class StPifStorageVarInfo
 * @brief Storage header that tracks variable storage layout and node lists.
 */
typedef struct StPifStorageVarInfo
{
	uint8_t magin_code[4];			// "pifs" signature
	uint8_t verion;					// format version (1 or higher)
	uint8_t data_info_count;
	uint16_t sector_size;
	uint16_t max_sector_count;

	uint16_t first_node;
	uint16_t free_node;
	uint16_t crc_16;
} PifStorageVarInfo;

/**
 * @class StPifStorageVar
 * @brief Variable-length storage manager using linked metadata nodes.
 */
typedef struct StPifStorageVar
{
	// The parent variable must be at the beginning of this structure.
	PifStorage parent;

	// Public Member Variable

	// Read-only Member Variable
	PifStorageVarInfo* _p_info;

	// Private Member Variable
    BOOL __is_format;
    uint32_t __info_bytes;
	uint16_t __info_sectors;
	uint8_t* __p_info_buffer;
	PifStorageVarDataInfo* __p_data_info;

	// Private Action Function
} PifStorageVar;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStorageVar_Init
 * @brief Initializes a variable storage manager instance.
 * @param p_owner Pointer to the variable storage instance.
 * @param id Object id, or `PIF_ID_AUTO` to allocate automatically.
 * @return `TRUE` on successful initialization, otherwise `FALSE`.
 */
BOOL pifStorageVar_Init(PifStorageVar* p_owner, PifId id);

/**
 * @fn pifStorageVar_Clear
 * @brief Releases dynamic buffers used by the variable storage manager.
 * @param p_owner Pointer to the variable storage instance.
 */
void pifStorageVar_Clear(PifStorageVar* p_owner);

/**
 * @fn pifStorageVar_AttachActStorage
 * @brief Attaches low-level media access callbacks to variable storage.
 * @param p_owner Pointer to the variable storage instance.
 * @param act_read Callback used for media reads.
 * @param act_write Callback used for media writes.
 * @return `TRUE` if callbacks are attached, otherwise `FALSE`.
 */
BOOL pifStorageVar_AttachActStorage(PifStorageVar* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);

/**
 * @fn pifStorageVar_AttachI2c
 * @brief Attaches an I2C-backed media device to variable storage.
 * @param p_owner Pointer to the variable storage instance.
 * @param p_port I2C port used for communication.
 * @param addr Device address.
 * @param i_addr_size Width of internal media address bytes.
 * @param write_delay_ms Delay after write transactions in milliseconds.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageVar_AttachI2c(PifStorageVar* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifStorageVar_SetMedia
 * @brief Configures media geometry and metadata capacity for variable storage.
 * @param p_owner Pointer to the variable storage instance.
 * @param sector_size Logical sector size in bytes.
 * @param storage_volume Total media capacity in bytes.
 * @param data_info_count Number of metadata nodes reserved in the header.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageVar_SetMedia(PifStorageVar* p_owner, uint16_t sector_size, uint32_t storage_volume, uint8_t data_info_count);

/**
 * @fn pifStorageVar_IsFormat
 * @brief Checks whether variable storage metadata is currently formatted.
 * @param p_parent Pointer to base storage interface.
 * @return `TRUE` if formatted, otherwise `FALSE`.
 */
BOOL pifStorageVar_IsFormat(PifStorage* p_parent);

/**
 * @fn pifStorageVar_Format
 * @brief Formats variable storage metadata and initializes free-node lists.
 * @param p_parent Pointer to base storage interface.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageVar_Format(PifStorage* p_parent);

/**
 * @fn pifStorageVar_Create
 * @brief Allocates a variable storage entry with given id and payload size.
 * @param p_parent Pointer to base storage interface.
 * @param id Logical data id to allocate.
 * @param size Payload size in bytes.
 * @return Data info handle on success, otherwise `NULL`.
 */
PifStorageDataInfoP pifStorageVar_Create(PifStorage* p_parent, uint16_t id, uint16_t size);

/**
 * @fn pifStorageVar_Delete
 * @brief Deletes a variable storage entry and returns its node to free list.
 * @param p_parent Pointer to base storage interface.
 * @param id Logical data id to delete.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageVar_Delete(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageVar_Open
 * @brief Opens an existing variable storage entry by id.
 * @param p_parent Pointer to base storage interface.
 * @param id Logical data id to open.
 * @return Data info handle on success, otherwise `NULL`.
 */
PifStorageDataInfoP pifStorageVar_Open(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageVar_Read
 * @brief Reads payload bytes from a variable storage entry.
 * @param p_parent Pointer to base storage interface.
 * @param p_dst Destination buffer.
 * @param p_src Source entry metadata handle.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageVar_Read(PifStorage* p_parent, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);

/**
 * @fn pifStorageVar_Write
 * @brief Writes payload bytes to a variable storage entry.
 * @param p_parent Pointer to base storage interface.
 * @param p_dst Destination entry metadata handle.
 * @param p_src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorageVar_Write(PifStorage* p_parent, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);

#if defined(PIF_DEBUG) && !defined(PIF_NO_LOG)

/**
 * @fn pifStorageVar_PrintInfo
 * @brief Prints variable storage metadata for debugging.
 * @param p_owner Pointer to the variable storage instance.
 * @param human `TRUE` to print readable summary, `FALSE` for raw hex blocks.
 */
void pifStorageVar_PrintInfo(PifStorageVar* p_owner, BOOL human);

/**
 * @fn pifStorageVar_Dump
 * @brief Dumps raw media bytes from the specified storage range.
 * @param p_owner Pointer to the variable storage instance.
 * @param pos Start position in bytes.
 * @param length Number of bytes to dump.
 */
void pifStorageVar_Dump(PifStorageVar* p_owner, uint32_t pos, uint32_t length);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_VAR_H
