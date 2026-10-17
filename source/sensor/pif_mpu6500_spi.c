// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "sensor/pif_mpu6500_spi.h"


BOOL pifMpu6500Spi_Detect(PifSpiPort* p_spi, void *p_client)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU6500 Ident: ";
#endif	
	uint8_t data;
	PifSpiDevice* p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi, p_client);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, MPU6500_REG_WHO_AM_I, &data)) return FALSE;
	if (data != MPU6500_WHO_AM_I_CONST) return FALSE;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "%s%Xh", ident, data);
#endif
	return TRUE;
}

BOOL pifMpu6500Spi_Init(PifMpu6500* p_owner, PifId id, PifSpiPort* p_spi, void *p_client, PifImuSensor* p_imu_sensor)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_spi || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu6500));

    p_device = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

	// On SPI the datasheet asks for SIGNAL_PATH_RESET after DEVICE_RESET, each followed by 100ms.
	// Initialization runs before there is anything to schedule, so the waits hold the CPU and
	// nothing is lost by them.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU6500_REG_PWR_MGMT_1, MPU6500_DEVICE_RESET(1))) goto fail;
	pif_Delay1ms(100);

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU6500_REG_SIGNAL_PATH_RESET,
			MPU6500_GYRO_RESET(1) | MPU6500_ACCEL_RESET(1) | MPU6500_TEMP_RESET(1))) goto fail;
	pif_Delay1ms(100);

	// The reset turns the I2C interface back on. Left on, noise on the bus pins can switch the
	// chip into I2C mode, so the datasheet asks to disable it when SPI is used.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU6500_REG_USER_CTRL, MPU6500_I2C_IF_DIS(1))) goto fail;

	if (!pifMpu6500_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifMpu6500Spi_Clear(p_owner);
	return FALSE;
}

void pifMpu6500Spi_Clear(PifMpu6500* p_owner)
{
    if (p_owner->_p_device) {
		PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;

		pifSpiPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    }
}
