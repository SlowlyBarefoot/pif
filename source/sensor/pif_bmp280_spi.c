// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_bmp280_spi.h"


BOOL pifBmp280Spi_Detect(PifSpiPort* p_spi)
{
	uint8_t data;
	PifSpiDevice* p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi, NULL);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, BMP280_REG_ID, &data)) return FALSE;
	if (data != BMP280_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmp280Spi_Init(PifBmp280* p_owner, PifId id, PifSpiPort* p_spi, void *p_client)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_spi) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmp280));

    p_device = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

    if (!pifBmp280_Config(p_owner, id)) goto fail;
    return TRUE;

fail:
	pifBmp280Spi_Clear(p_owner);
	return FALSE;
}

void pifBmp280Spi_Clear(PifBmp280* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->_p_device) {
		PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;

		pifSpiPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
	}
}
