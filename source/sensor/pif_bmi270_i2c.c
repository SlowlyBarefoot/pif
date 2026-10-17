// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_bmi270_i2c.h"


BOOL pifBmi270I2c_Detect(PifI2cPort *p_i2c, uint8_t addr, void *p_client)
{
	uint8_t data;
	PifI2cDevice *p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr, p_client);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, BMI270_REG_CHIP_ID, &data)) return FALSE;
	if (data != BMI270_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmi270I2c_Init(PifBmi270 *p_owner, PifId id, PifI2cPort *p_i2c, uint8_t addr, void *p_client, PifImuSensor *p_imu_sensor)
{
	PifI2cDevice *p_device;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmi270));

    p_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

	if (!pifBmi270_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifBmi270I2c_Clear(p_owner);
	return FALSE;
}

void pifBmi270I2c_Clear(PifBmi270 *p_owner)
{
    if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    }
}
