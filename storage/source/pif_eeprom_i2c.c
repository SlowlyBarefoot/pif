#include "pif_eeprom_i2c.h"
#include "pif_task.h"


static BOOL _actStorageRead_1(uint8_t* dst, uint32_t src, size_t size, void* p_issuer)
{
	PifEepromI2c* p_owner = (PifEepromI2c*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (src >> 8);
	if (!pifI2cDevice_Read(p_owner->_p_i2c, src & 0xFF, 1, dst, size)) return FALSE;
	return TRUE;
}

static BOOL _actStorageWrite_1(uint32_t dst, uint8_t* src, size_t size, void* p_issuer)
{
	PifEepromI2c* p_owner = (PifEepromI2c*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (dst >> 8);
	if (!pifI2cDevice_Write(p_owner->_p_i2c, dst & 0xFF, 1, src, size)) return FALSE;
	if (p_owner->__write_delay_ms) pifTaskManager_YieldMs(p_owner->__write_delay_ms);
	return TRUE;
}

static BOOL _actStorageRead_2(uint8_t* dst, uint32_t src, size_t size, void* p_issuer)
{
	PifEepromI2c* p_owner = (PifEepromI2c*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (src >> 16);
	if (!pifI2cDevice_Read(p_owner->_p_i2c, src & 0xFFFF, 2, dst, size)) return FALSE;
	return TRUE;
}

static BOOL _actStorageWrite_2(uint32_t dst, uint8_t* src, size_t size, void* p_issuer)
{
	PifEepromI2c* p_owner = (PifEepromI2c*)p_issuer;

	p_owner->_p_i2c->addr = p_owner->__addr | (dst >> 16);
	if (!pifI2cDevice_Write(p_owner->_p_i2c, dst & 0xFFFF, 2, src, size)) return FALSE;
	if (p_owner->__write_delay_ms) pifTaskManager_YieldMs(p_owner->__write_delay_ms);
	return TRUE;
}

BOOL pifEepromI2c_Init(PifEepromI2c* p_owner, PifId id, uint8_t min_data_info_count, uint16_t sector_size, uint32_t storage_volume,
		PifI2cPort* p_port, uint8_t addr, PifEepromI2cIAddrSize i_addr_size, uint8_t write_delay_ms)
{
	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifEepromI2c));

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->_p_i2c = pifI2cPort_AddDevice(p_port);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->__addr = addr;
    p_owner->_p_i2c->addr = addr;
    p_owner->__write_delay_ms = write_delay_ms;

    switch (i_addr_size) {
    case EEPROM_I2C_I_ADDR_SIZE_1:
    	if (!pifStorage_Init(&p_owner->_storage, PIF_ID_AUTO, min_data_info_count, sector_size, storage_volume,
    			_actStorageRead_1, _actStorageWrite_1, p_owner)) goto fail;
    	break;

    case EEPROM_I2C_I_ADDR_SIZE_2:
    	if (!pifStorage_Init(&p_owner->_storage, PIF_ID_AUTO, min_data_info_count, sector_size, storage_volume,
    			_actStorageRead_2, _actStorageWrite_2, p_owner)) goto fail;
    	break;

    default:
    	goto fail;
    }
    return TRUE;

fail:
	pifEepromI2c_Clear(p_owner);
	return FALSE;
}

void pifEepromI2c_Clear(PifEepromI2c* p_owner)
{
	pifStorage_Clear(&p_owner->_storage);
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}
