// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "sensor/pif_mpu60x0_i2c.h"


BOOL pifMpu60x0I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU60X0 Ident: ";
#endif	
	uint8_t data;
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr, p_client);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, MPU60X0_REG_WHO_AM_I, &data)) return FALSE;
	// WHO_AM_I does not follow the AD0 pin, so it is the same at either address.
	if (data != MPU60X0_WHO_AM_I_CONST) return FALSE;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "%s%Xh", ident, data);
#endif
	return TRUE;
}

BOOL pifMpu60x0I2c_Init(PifMpu60x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor)
{
	PifI2cDevice *p_device;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu60x0));

    p_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU60X0_REG_PWR_MGMT_1, MPU60X0_DEVICE_RESET(1))) goto fail;
	// The reset needs this long before any register may be read. Initialization runs before there
	// is anything to schedule, so the wait holds the CPU and nothing is lost by it.
	pif_Delay1ms(100);

	if (!pifMpu60x0_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifMpu60x0I2c_Clear(p_owner);
	return FALSE;
}

void pifMpu60x0I2c_Clear(PifMpu60x0* p_owner)
{
    if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    }
}
