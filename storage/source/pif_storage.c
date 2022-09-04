#include "pif_storage.h"
#include "pif_task.h"


static BOOL _actStorageI2c_Read_1(uint8_t* dst, uint32_t src, size_t size, void* p_issuer)
{
	PifStorage* p_owner = (PifStorage*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (src >> 8);
	if (!pifI2cDevice_Read(p_owner->_p_i2c, src & 0xFF, 1, dst, size)) return FALSE;
	return TRUE;
}

static BOOL _actStorageI2c_Write_1(uint32_t dst, uint8_t* src, size_t size, void* p_issuer)
{
	PifStorage* p_owner = (PifStorage*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (dst >> 8);
	if (!pifI2cDevice_Write(p_owner->_p_i2c, dst & 0xFF, 1, src, size)) return FALSE;
	if (p_owner->__write_delay_ms) pifTaskManager_YieldMs(p_owner->__write_delay_ms);
	return TRUE;
}

static BOOL _actStorageI2c_Read_2(uint8_t* dst, uint32_t src, size_t size, void* p_issuer)
{
	PifStorage* p_owner = (PifStorage*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (src >> 16);
	if (!pifI2cDevice_Read(p_owner->_p_i2c, src & 0xFFFF, 2, dst, size)) return FALSE;
	return TRUE;
}

static BOOL _actStorageI2c_Write_2(uint32_t dst, uint8_t* src, size_t size, void* p_issuer)
{
	PifStorage* p_owner = (PifStorage*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (dst >> 16);
	if (!pifI2cDevice_Write(p_owner->_p_i2c, dst & 0xFFFF, 2, src, size)) return FALSE;
	if (p_owner->__write_delay_ms) pifTaskManager_YieldMs(p_owner->__write_delay_ms);
	return TRUE;
}

BOOL pifStorage_AttachActStorage(PifStorage* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write)
{
    if (!p_owner || !act_read || !act_write) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__act_read = act_read;
	p_owner->__act_write = act_write;
	p_owner->__p_issuer = p_owner;
	return TRUE;
}

BOOL pifStorage_AttachI2c(PifStorage* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms)
{
    if (!p_owner || !p_port) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->_p_i2c = pifI2cPort_AddDevice(p_port);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->__addr = addr;
    p_owner->_p_i2c->addr = addr;
    p_owner->__write_delay_ms = write_delay_ms;

    switch (i_addr_size) {
    case SIC_I_ADDR_SIZE_1:
		p_owner->__act_read = _actStorageI2c_Read_1;
		p_owner->__act_write = _actStorageI2c_Write_1;
		p_owner->__p_issuer = p_owner;
    	break;

    case SIC_I_ADDR_SIZE_2:
		p_owner->__act_read = _actStorageI2c_Read_2;
		p_owner->__act_write = _actStorageI2c_Write_2;
		p_owner->__p_issuer = p_owner;
    	break;

    default:
    	goto fail;
    }
	return TRUE;

fail:
	pifStorage_DetachI2c(p_owner);
	return FALSE;
}

void pifStorage_DetachI2c(PifStorage* p_owner)
{
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
	p_owner->__act_read = NULL;
	p_owner->__act_write = NULL;
}

#ifdef __PIF_NO_USE_INLINE__

BOOL pifStorage_IsFormat(PifStorage* p_owner)
{
	return (*p_owner->__fn_is_format)(p_owner);
}

BOOL pifStorage_Format(PifStorage* p_owner)
{
	return (*p_owner->__fn_format)(p_owner);
}

PifStorageDataInfo* pifStorage_Create(PifStorage* p_owner, uint16_t id, uint16_t size)
{
	return (*p_owner->__fn_create)(p_owner, id, size);
}

BOOL pifStorage_Delete(PifStorage* p_owner, uint16_t id)
{
	return (*p_owner->__fn_delete)(p_owner, id);
}

PifStorageDataInfo* pifStorage_Open(PifStorage* p_owner, uint16_t id)
{
	return (*p_owner->__fn_open)(p_owner, id);
}

BOOL pifStorage_Read(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfo* p_src, size_t size)
{
	return (*p_owner->__fn_read)(p_owner, p_dst, p_src, size);
}

BOOL pifStorage_Write(PifStorage* p_owner, PifStorageDataInfo* p_dst, uint8_t* p_src, size_t size)
{
	return (*p_owner->__fn_write)(p_owner, p_dst, p_src, size);
}

#endif
