#include "sensor/pif_bmi270_i2c.h"


BOOL pifBmi270I2c_Detect(PifI2cPort *p_i2c, uint8_t addr, void *p_client)
{
	uint8_t data;
	PifI2cDevice *p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr, p_client);

	if (!pifI2cDevice_ReadRegByte(p_device, BMI270_REG_WHO_AM_I | 0x80, &data)) return FALSE;
	if (data != BMI270_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmi270I2c_Init(PifBmi270 *p_owner, PifId id, PifI2cPort *p_i2c, uint8_t addr, void *p_client, PifImuSensor *p_imu_sensor)
{
	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmi270));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_fn.p_device = p_owner->_p_i2c;

	p_owner->_fn.read_byte = pifI2cDevice_ReadRegByte;
	p_owner->_fn.read_bytes = pifI2cDevice_ReadRegBytes;
	p_owner->_fn.read_bit = pifI2cDevice_ReadRegBit8;

	p_owner->_fn.write_byte = pifI2cDevice_WriteRegByte;
	p_owner->_fn.write_bytes = pifI2cDevice_WriteRegBytes;
	p_owner->_fn.write_bit = pifI2cDevice_WriteRegBit8;

	if (!pifBmi270_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifBmi270I2c_Clear(p_owner);
	return FALSE;
}

void pifBmi270I2c_Clear(PifBmi270 *p_owner)
{
    if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
    }
}
