#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "storage/pif_storage_fix.h"


BOOL pifStorageFix_Init(PifStorageFix* p_owner, PifId id)
{
    if (!p_owner) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifStorageFix));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

	p_owner->parent.__fn_is_format = pifStorageFix_IsFormat;
	p_owner->parent.__fn_format = pifStorageFix_Format;
	p_owner->parent.__fn_create = pifStorageFix_Create;
	p_owner->parent.__fn_delete = pifStorageFix_Delete;
	p_owner->parent.__fn_open = pifStorageFix_Open;
	p_owner->parent.__fn_read = pifStorageFix_Read;
	p_owner->parent.__fn_write = pifStorageFix_Write;
	return TRUE;
}

void pifStorageFix_Clear(PifStorageFix* p_owner)
{
    if (p_owner->__p_data_info) {
        free(p_owner->__p_data_info);
        p_owner->__p_data_info = NULL;
    }
	p_owner->parent.__fn_is_format = NULL;
	p_owner->parent.__fn_format = NULL;
	p_owner->parent.__fn_create = NULL;
	p_owner->parent.__fn_delete = NULL;
	p_owner->parent.__fn_open = NULL;
	p_owner->parent.__fn_read = NULL;
	p_owner->parent.__fn_write = NULL;
}

BOOL pifStorageFix_SetMedia(PifStorageFix* p_owner, uint32_t sector_size, uint32_t storage_volume)
{
	uint16_t count, i;

	if (!p_owner || sector_size < 16 || !storage_volume) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	count = storage_volume / sector_size;
    p_owner->__p_data_info = calloc(sizeof(PifStorageFixDataInfo), count);
    if (!p_owner->__p_data_info) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}

	p_owner->__sector_size = sector_size;
	p_owner->parent._storage_volume = storage_volume;
	p_owner->__data_info_count = count;
	for (i = 0; i < count; i++) {
		p_owner->__p_data_info[i].id = i;
	}
	return TRUE;
}

PIF_INLINE BOOL pifStorageFix_AttachActStorage(PifStorageFix* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write)
{
	return pifStorage_AttachActStorage(&p_owner->parent, act_read, act_write);
}

PIF_INLINE BOOL pifStorageFix_AttachI2c(PifStorageFix* p_owner, PifI2cPort* p_port, uint8_t addr, uint16_t max_transfer_size, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms)
{
	return pifStorage_AttachI2c(&p_owner->parent, p_port, addr, max_transfer_size, i_addr_size, write_delay_ms);
}

BOOL pifStorageFix_IsFormat(PifStorage* p_parent)
{
	(void)p_parent;

	return TRUE;
}

BOOL pifStorageFix_Format(PifStorage* p_parent)
{
	(void)p_parent;

	return TRUE;
}

PifStorageDataInfoP pifStorageFix_Create(PifStorage* p_parent, uint16_t id, uint16_t size)
{
	(void)size;

	return pifStorageFix_Open(p_parent, id);
}

BOOL pifStorageFix_Delete(PifStorage* p_parent, uint16_t id)
{
	(void)p_parent;
	(void)id;

	return TRUE;
}

PifStorageDataInfoP pifStorageFix_Open(PifStorage* p_parent, uint16_t id)
{
	PifStorageFix* p_owner = (PifStorageFix*)p_parent;

	if (id >= p_owner->__data_info_count) {
		pif_error = E_IS_NOT_FORMATED;
		return NULL;
	}

	return (PifStorageDataInfoP)(p_owner->__p_data_info + id);
}

BOOL pifStorageFix_Read(PifStorage* p_parent, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size)
{
	PifStorageFix* p_owner = (PifStorageFix*)p_parent;

	return (*p_owner->parent.__act_read)(p_parent, p_dst, ((PifStorageFixDataInfo*)p_src)->id * p_owner->__sector_size, size);
}

BOOL pifStorageFix_Write(PifStorage* p_parent, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size)
{
	PifStorageFix* p_owner = (PifStorageFix*)p_parent;

	return (*p_owner->parent.__act_write)(p_parent, ((PifStorageFixDataInfo*)p_dst)->id * p_owner->__sector_size, p_src, size);
}
