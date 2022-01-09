#ifndef PIF_STORAGE_H
#define PIF_STORAGE_H


#include "pif.h"


typedef BOOL (*PifActStorageRead)(uint8_t* dst, uint32_t src, size_t size, void* p_issuer);
typedef BOOL (*PifActStorageWrite)(uint32_t src, uint8_t* dst, size_t size, void* p_issuer);


/**
 * @class StPifStorageDataInfo
 * @brief
 */
typedef struct StPifStorageDataInfo
{
	uint16_t id;
	uint16_t size;
	uint16_t first_sector;

	uint16_t next_node;
	uint16_t prev_node;
	uint16_t crc_16;
} PifStorageDataInfo;

/**
 * @class StPifStorageInfo
 * @brief
 */
typedef struct StPifStorageInfo
{
	uint8_t magin_code[4];			// "pifs"
	uint8_t verion;					// 1 ~
	uint8_t max_data_info_count;
	uint16_t sector_size;
	uint16_t max_sector_count;

	uint16_t first_node;
	uint16_t free_node;
	uint16_t crc_16;
} PifStorageInfo;

/**
 * @class StPifStorage
 * @brief
 */
typedef struct StPifStorage
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;
    BOOL _is_format;
	PifStorageInfo* _p_info;

	// Private Member Variable
    uint32_t __info_bytes;
	uint16_t __info_sectors;
	uint8_t* __p_info_buffer;
	PifStorageDataInfo* __p_data_info;
	void* __p_issuer;

	// Private Action Function
	PifActStorageRead __act_read;
	PifActStorageWrite __act_write;
} PifStorage;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStorage_Init
 * @brief
 * @param p_owner
 * @param id
 * @param min_data_info_count
 * @param sector_size
 * @param storage_volume
 * @param act_read
 * @param act_write
 * @param p_issuer
 * @return
 */
BOOL pifStorage_Init(PifStorage* p_owner, PifId id, uint8_t min_data_info_count, uint16_t sector_size, uint32_t storage_volume,
		PifActStorageRead act_read, PifActStorageWrite act_write, void* p_issuer);

/**
 * @fn pifStorage_Clear
 * @brief
 * @param p_owner
 */
void pifStorage_Clear(PifStorage* p_owner);

/**
 * @fn pifStorage_Format
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifStorage_Format(PifStorage* p_owner);

/**
 * @fn pifStorage_Alloc
 * @brief
 * @param p_owner
 * @param id
 * @param size
 * @return
 */
PifStorageDataInfo* pifStorage_Alloc(PifStorage* p_owner, uint16_t id, uint16_t size);

/**
 * @fn pifStorage_Free
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifStorage_Free(PifStorage* p_owner, uint16_t id);

/**
 * @fn pifStorage_GetDataInfo
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
PifStorageDataInfo* pifStorage_GetDataInfo(PifStorage* p_owner, uint16_t id);

/**
 * @fn pifStorage_Read
 * @brief
 * @param p_owner
 * @param p_data_info
 * @param p_data
 * @return
 */
BOOL pifStorage_Read(PifStorage* p_owner, PifStorageDataInfo* p_data_info, uint8_t* p_data);

/**
 * @fn pifStorage_Write
 * @brief
 * @param p_owner
 * @param p_data_info
 * @param p_data
 * @return
 */
BOOL pifStorage_Write(PifStorage* p_owner, PifStorageDataInfo* p_data_info, uint8_t* p_data);

#if defined(__PIF_DEBUG__) && !defined(__PIF_NO_LOG__)

/**
 * @fn pifStorage_PrintInfo
 * @brief
 * @param p_owner
 * @param human
 */
void pifStorage_PrintInfo(PifStorage* p_owner, BOOL human);

/**
 * @fn pifStorage_Dump
 * @brief
 * @param p_owner
 * @param pos
 * @param length
 */
void pifStorage_Dump(PifStorage* p_owner, uint32_t pos, uint32_t length);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_H
