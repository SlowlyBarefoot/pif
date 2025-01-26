#include "core/pif_log.h"
#include "sensor/pif_mpu6500_i2c.h"


BOOL pifMpu6500I2c_Detect(PifI2cPort* p_i2c, uint8_t addr)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU6500 Ident: ";
#endif	
	uint8_t data;
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr);

	if (!pifI2cDevice_ReadRegByte(p_device, MPU6500_REG_WHO_AM_I, &data)) return FALSE;
	if (data != MPU6500_WHO_AM_I_CONST) return FALSE;
#ifndef PIF_NO_LOG	
	if (data < 32) {
		pifLog_Printf(LT_INFO, "%s%Xh", ident, data >> 1);
	}
	else {
		pifLog_Printf(LT_INFO, "%s%c", ident, data >> 1);
	}
#endif
	return TRUE;
}

BOOL pifMpu6500I2c_Init(PifMpu6500* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, uint16_t max_transfer_size, PifImuSensor* p_imu_sensor)
{
	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu6500));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, max_transfer_size);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_fn.p_device = p_owner->_p_i2c;

	p_owner->_fn.read_byte = pifI2cDevice_ReadRegByte;
	p_owner->_fn.read_bytes = pifI2cDevice_ReadRegBytes;
	p_owner->_fn.read_bit = pifI2cDevice_ReadRegBit8;

	p_owner->_fn.write_byte = pifI2cDevice_WriteRegByte;
	p_owner->_fn.write_bytes = pifI2cDevice_WriteRegBytes;
	p_owner->_fn.write_bit = pifI2cDevice_WriteRegBit8;

	if (!pifMpu6500_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifMpu6500I2c_Clear(p_owner);
	return FALSE;
}

void pifMpu6500I2c_Clear(PifMpu6500* p_owner)
{
    if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
    }
}
