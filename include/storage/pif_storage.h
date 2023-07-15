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
 * @brief
 */
struct StPifStorage
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;
	PifI2cDevice* _p_i2c;

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
 * @brief
 * @param p_owner
 * @param act_read
 * @param act_write
 * @return
 */
BOOL pifStorage_AttachActStorage(PifStorage* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);

/**
 * @fn pifStorage_AttachI2c
 * @brief
 * @param p_owner
 * @param p_port
 * @param addr
 * @param i_addr_size
 * @param write_delay_ms
 * @return
 */
BOOL pifStorage_AttachI2c(PifStorage* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifStorage_DetachI2c
 * @brief
 * @param p_owner
 */
void pifStorage_DetachI2c(PifStorage* p_owner);

/**
 * @fn pifStorage_IsFormat
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorage_IsFormat(PifStorage* p_owner);
#else
	inline BOOL pifStorage_IsFormat(PifStorage* p_owner) {
		return (*p_owner->__fn_is_format)(p_owner);
	}
#endif

/**
 * @fn pifStorage_Format
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorage_Format(PifStorage* p_owner);
#else
	inline BOOL pifStorage_Format(PifStorage* p_owner)	{
		return (*p_owner->__fn_format)(p_owner);
	}
#endif

/**
 * @fn pifStorage_Create
 * @brief
 * @param p_owner
 * @param id
 * @param size
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifStorageDataInfoP pifStorage_Create(PifStorage* p_owner, uint16_t id, uint16_t size);
#else
	inline PifStorageDataInfoP pifStorage_Create(PifStorage* p_owner, uint16_t id, uint16_t size) {
		return (*p_owner->__fn_create)(p_owner, id, size);
	}
#endif

/**
 * @fn pifStorage_Delete
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorage_Delete(PifStorage* p_owner, uint16_t id);
#else
	inline BOOL pifStorage_Delete(PifStorage* p_owner, uint16_t id) {
		return (*p_owner->__fn_delete)(p_owner, id);
	}
#endif

/**
 * @fn pifStorage_Open
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifStorageDataInfoP pifStorage_Open(PifStorage* p_owner, uint16_t id);
#else
	inline PifStorageDataInfoP pifStorage_Open(PifStorage* p_owner, uint16_t id) {
		return (*p_owner->__fn_open)(p_owner, id);
	}
#endif

/**
 * @fn pifStorage_Read
 * @brief
 * @param p_owner
 * @param p_dst
 * @param p_src
 * @param size
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorage_Read(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);
#else
	inline BOOL pifStorage_Read(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size) {
		return (*p_owner->__fn_read)(p_owner, p_dst, p_src, size);
	}
#endif

/**
 * @fn pifStorage_Write
 * @brief
 * @param p_owner
 * @param p_dst
 * @param p_src
 * @param size
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	BOOL pifStorage_Write(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);
#else
	inline BOOL pifStorage_Write(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size) {
		return (*p_owner->__fn_write)(p_owner, p_dst, p_src, size);
	}
#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_H
