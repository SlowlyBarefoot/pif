#ifndef PIF_STORAGE_FIX_H
#define PIF_STORAGE_FIX_H


#include "storage/pif_storage.h"


/**
 * @class StPifStorageFixDataInfo
 * @brief
 */
typedef struct StPifStorageFixDataInfo
{
	uint16_t id;
} PifStorageFixDataInfo;

/**
 * @class StPifStorageFix
 * @brief
 */
typedef struct StPifStorageFix
{
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
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifStorageFix_Init(PifStorageFix* p_owner, PifId id);

/**
 * @fn pifStorageFix_Clear
 * @brief
 * @param p_owner
 */
void pifStorageFix_Clear(PifStorageFix* p_owner);

/**
 * @fn pifStorageFix_AttachActStorage
 * @brief
 * @param p_owner
 * @param act_read
 * @param act_write
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorageFix_AttachActStorage(PifStorageFix* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);
#else
	inline BOOL pifStorageFix_AttachActStorage(PifStorageFix* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write) {
		return pifStorage_AttachActStorage(&p_owner->parent, act_read, act_write);
	}
#endif

/**
 * @fn pifStorageFix_AttachI2c
 * @brief
 * @param p_owner
 * @param p_port
 * @param addr
 * @param i_addr_size
 * @param write_delay_ms
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorageFix_AttachI2c(PifStorageFix* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);
#else
	inline BOOL pifStorageFix_AttachI2c(PifStorageFix* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms) {
		return pifStorage_AttachI2c(&p_owner->parent, p_port, addr, i_addr_size, write_delay_ms);
	}
#endif

/**
 * @fn pifStorageFix_SetMedia
 * @brief
 * @param p_owner
 * @param sector_size
 * @param storage_volume
 * @return
 */
BOOL pifStorageFix_SetMedia(PifStorageFix* p_owner, uint32_t sector_size, uint32_t storage_volume);

/**
 * @fn pifStorageFix_IsFormat
 * @brief
 * @param p_parent
 * @return
 */
BOOL pifStorageFix_IsFormat(PifStorage* p_parent);

/**
 * @fn pifStorageFix_Format
 * @brief
 * @param p_parent
 * @return
 */
BOOL pifStorageFix_Format(PifStorage* p_parent);

/**
 * @fn pifStorageFix_Create
 * @brief
 * @param p_parent
 * @param id
 * @param size
 * @return
 */
PifStorageDataInfoP pifStorageFix_Create(PifStorage* p_parent, uint16_t id, uint16_t size);

/**
 * @fn pifStorageFix_Delete
 * @brief
 * @param p_parent
 * @param id
 * @return
 */
BOOL pifStorageFix_Delete(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageFix_Open
 * @brief
 * @param p_parent
 * @param id
 * @return
 */
PifStorageDataInfoP pifStorageFix_Open(PifStorage* p_parent, uint16_t id);

/**
 * @fn pifStorageFix_Read
 * @brief
 * @param p_parent
 * @param p_dst
 * @param p_src
 * @param size
 * @return
 */
BOOL pifStorageFix_Read(PifStorage* p_parent, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);

/**
 * @fn pifStorageFix_Write
 * @brief
 * @param p_parent
 * @param p_dst
 * @param p_src
 * @param size
 * @return
 */
BOOL pifStorageFix_Write(PifStorage* p_parent, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_FIX_H
