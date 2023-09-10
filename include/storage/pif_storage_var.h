#ifndef PIF_STORAGE_VAR_H
#define PIF_STORAGE_VAR_H


#include "storage/pif_storage.h"


/**
 * @class StPifStorageVarDataInfo
 * @brief
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
 * @brief
 */
typedef struct StPifStorageVarInfo
{
	uint8_t magin_code[4];			// "pifs"
	uint8_t verion;					// 1 ~
	uint8_t data_info_count;
	uint16_t sector_size;
	uint16_t max_sector_count;

	uint16_t first_node;
	uint16_t free_node;
	uint16_t crc_16;
} PifStorageVarInfo;

/**
 * @class StPifStorageVar
 * @brief
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
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifStorageVar_Init(PifStorageVar* p_owner, PifId id);

/**
 * @fn pifStorageVar_Clear
 * @brief
 * @param p_owner
 */
void pifStorageVar_Clear(PifStorageVar* p_owner);

/**
 * @fn pifStorageVar_AttachActStorage
 * @brief
 * @param p_owner
 * @param act_read
 * @param act_write
 * @return
 */
BOOL pifStorageVar_AttachActStorage(PifStorageVar* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);

/**
 * @fn pifStorageVar_AttachI2c
 * @brief
 * @param p_owner
 * @param p_port
 * @param addr
 * @param i_addr_size
 * @param write_delay_ms
 * @return
 */
BOOL pifStorageVar_AttachI2c(PifStorageVar* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifStorageVar_SetMedia
 * @brief
 * @param p_owner
 * @param sector_size
 * @param storage_volume
 * @param data_info_count
 * @return
 */
BOOL pifStorageVar_SetMedia(PifStorageVar* p_owner, uint16_t sector_size, uint32_t storage_volume, uint8_t data_info_count);

/**
 * @fn pifStorageVar_IsFormat
 * @brief
 * @param p_parent
 * @return
 */
BOOL pifStorageVar_IsFormat(PifStorage* p_parent);

/**
 * @fn pifStorageVar_Format
 * @brief
 * @param p_parent
 * @return
 */
BOOL pifStorageVar_Format(PifStorage* p_parent);

/**
 * @fn pifStorageVar_Create
 * @brief
 * @param p_parent
 * @param id
 * @param size
 * @return
 */
PifStorageDataInfoP pifStorageVar_Create(PifStorage* p_parent, uint16_t id, uint16_t size);

/**
 * @fn pifStorageVar_Delete
 * @brief
 * @param p_parent
 * @param id
 * @return
 */
BOOL pifStorageVar_Delete(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageVar_Open
 * @brief
 * @param p_parent
 * @param id
 * @return
 */
PifStorageDataInfoP pifStorageVar_Open(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageVar_Read
 * @brief
 * @param p_parent
 * @param p_dst
 * @param p_src
 * @param size
 * @return
 */
BOOL pifStorageVar_Read(PifStorage* p_parent, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);

/**
 * @fn pifStorageVar_Write
 * @brief
 * @param p_parent
 * @param p_dst
 * @param p_src
 * @param size
 * @return
 */
BOOL pifStorageVar_Write(PifStorage* p_parent, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);

#if defined(PIF_DEBUG) && !defined(PIF_NO_LOG)

/**
 * @fn pifStorageVar_PrintInfo
 * @brief
 * @param p_owner
 * @param human
 */
void pifStorageVar_PrintInfo(PifStorageVar* p_owner, BOOL human);

/**
 * @fn pifStorageVar_Dump
 * @brief
 * @param p_owner
 * @param pos
 * @param length
 */
void pifStorageVar_Dump(PifStorageVar* p_owner, uint32_t pos, uint32_t length);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_VAR_H
