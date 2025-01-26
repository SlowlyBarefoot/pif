#include "core/pif_task.h"
#include "sensor/pif_bmp280_i2c.h"


BOOL pifBmp280I2c_Detect(PifI2cPort* p_i2c, uint8_t addr)
{
	uint8_t data;
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr);

	if (!pifI2cDevice_ReadRegByte(p_device, BMP280_REG_ID, &data)) return FALSE;
	if (data != BMP280_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmp280I2c_Init(PifBmp280* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, uint16_t max_transfer_size)
{
	if (!p_owner || !p_i2c) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmp280));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, max_transfer_size);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_fn.p_device = p_owner->_p_i2c;

	p_owner->_fn.read_byte = pifI2cDevice_ReadRegByte;
	p_owner->_fn.read_bytes = pifI2cDevice_ReadRegBytes;
	p_owner->_fn.read_bit = pifI2cDevice_ReadRegBit8;

	p_owner->_fn.write_byte = pifI2cDevice_WriteRegByte;
	p_owner->_fn.write_bytes = pifI2cDevice_WriteRegBytes;
	p_owner->_fn.write_bit = pifI2cDevice_WriteRegBit8;

    if (!pifBmp280_Config(p_owner, id)) goto fail;
    return TRUE;

fail:
	pifBmp280I2c_Clear(p_owner);
	return FALSE;
}

void pifBmp280I2c_Clear(PifBmp280* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}
