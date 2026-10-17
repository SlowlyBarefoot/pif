// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_bmi270_spi.h"


BOOL pifBmi270Spi_Detect(PifSpiPort *p_spi, void *p_client)
{
	uint8_t data[2];
	PifSpiDevice *p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi, p_client);

	// The chip listens on I2C until the rising edge of CSB at the end of a read switches it to
	// SPI, so what this first read returns is not to be trusted.
	if (!pifDeviceReg_ReadRegBytes((PifDeviceReg *)p_device, BMI270_REG_CHIP_ID | 0x80, data, 2)) return FALSE;
	if (!pifDeviceReg_ReadRegBytes((PifDeviceReg *)p_device, BMI270_REG_CHIP_ID | 0x80, data, 2)) return FALSE;
	if (data[1] != BMI270_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmi270Spi_Init(PifBmi270 *p_owner, PifId id, PifSpiPort *p_spi, void *p_client, PifImuSensor *p_imu_sensor)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_spi || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmi270));

    p_device = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;
	p_owner->__dummy_read = TRUE;

	if (!pifBmi270_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifBmi270Spi_Clear(p_owner);
	return FALSE;
}

void pifBmi270Spi_Clear(PifBmi270 *p_owner)
{
    if (p_owner->_p_device) {
		PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;

		pifSpiPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    }
}
