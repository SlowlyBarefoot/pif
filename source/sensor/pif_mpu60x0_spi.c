// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "sensor/pif_mpu60x0_spi.h"


// PRODUCT_ID: the high nibble is the product, the low nibble the revision.
static const uint8_t c_mpu6000_product_id[] = {
	0x14, 0x15, 0x16, 0x17, 0x18,					// MPU-6000ES rev C4, C5, D6, D7, D8
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A		// MPU-6000 rev C4, C5, D6, D7, D8, D9, D10
};

BOOL pifMpu60x0Spi_Detect(PifSpiPort* p_spi, void *p_client)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU6000 Ident: ";
#endif	
	uint8_t data;
	size_t i;
	PifSpiDevice* p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi, p_client);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, MPU60X0_REG_WHO_AM_I, &data)) return FALSE;
	if (data != MPU60X0_WHO_AM_I_CONST) return FALSE;

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, MPU60X0_REG_PRODUCT_ID, &data)) return FALSE;
	for (i = 0; i < sizeof(c_mpu6000_product_id); i++) {
		if (data == c_mpu6000_product_id[i]) {
#ifndef PIF_NO_LOG	
			pifLog_Printf(LT_INFO, "%s%Xh", ident, data);
#endif
			return TRUE;
		}
	}
	return FALSE;
}

BOOL pifMpu60x0Spi_Init(PifMpu60x0* p_owner, PifId id, PifSpiPort* p_spi, void *p_client, PifImuSensor* p_imu_sensor)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_spi || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu60x0));

    p_device = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

	// On SPI the datasheet asks for SIGNAL_PATH_RESET after DEVICE_RESET, each followed by 100ms.
	// Initialization runs before there is anything to schedule, so the waits hold the CPU and
	// nothing is lost by them.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU60X0_REG_PWR_MGMT_1, MPU60X0_DEVICE_RESET(1))) goto fail;
	pif_Delay1ms(100);

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU60X0_REG_SIGNAL_PATH_RESET,
			MPU60X0_GYRO_RESET(1) | MPU60X0_ACCEL_RESET(1) | MPU60X0_TEMP_RESET(1))) goto fail;
	pif_Delay1ms(100);

	// The reset turns the I2C interface back on. Left on, noise on the bus pins can switch the
	// chip into I2C mode, so the datasheet asks to disable it when SPI is used.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU60X0_REG_USER_CTRL, MPU60X0_I2C_IF_DIS(1))) goto fail;

	if (!pifMpu60x0_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifMpu60x0Spi_Clear(p_owner);
	return FALSE;
}

void pifMpu60x0Spi_Clear(PifMpu60x0* p_owner)
{
    if (p_owner->_p_device) {
		PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;

		pifSpiPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    }
}
