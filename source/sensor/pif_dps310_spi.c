// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_dps310_spi.h"


BOOL pifDps310Spi_Detect(PifSpiPort* p_spi, void *p_client)
{
	uint8_t data;
	PifSpiDevice* p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi, p_client);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, DPS310_REG_PRODUCT_ID, &data)) return FALSE;
	if (data != DPS310_PRODUCT_ID_CONST) return FALSE;
	return TRUE;
}

BOOL pifDps310Spi_Init(PifDps310* p_owner, PifId id, PifSpiPort* p_spi, void *p_client)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_spi) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifDps310));

    p_device = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

    if (!pifDps310_Config(p_owner, id)) goto fail;
    return TRUE;

fail:
	pifDps310Spi_Clear(p_owner);
	return FALSE;
}

void pifDps310Spi_Clear(PifDps310* p_owner)
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
