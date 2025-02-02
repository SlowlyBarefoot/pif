#include "core/pif_task.h"
#include "sensor/pif_bmp280_spi.h"


BOOL pifBmp280Spi_Detect(PifSpiPort* p_spi)
{
	uint8_t data;
	PifSpiDevice* p_device;

    p_device = piSpicPort_TemporaryDevice(p_spi);

	if (!pifSpiDevice_ReadRegByte(p_device, BMP280_REG_ID, &data)) return FALSE;
	if (data != BMP280_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmp280Spi_Init(PifBmp280* p_owner, PifId id, PifSpiPort* p_spi, void *p_client)
{
	if (!p_owner || !p_spi) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmp280));

    p_owner->_p_spi = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_owner->_p_spi) return FALSE;

    p_owner->_fn.p_device = p_owner->_p_spi;

	p_owner->_fn.read_byte = pifSpiDevice_ReadRegByte;
	p_owner->_fn.read_bytes = pifSpiDevice_ReadRegBytes;
	p_owner->_fn.read_bit = pifSpiDevice_ReadRegBit8;

	p_owner->_fn.write_byte = pifSpiDevice_WriteRegByte;
	p_owner->_fn.write_bytes = pifSpiDevice_WriteRegBytes;
	p_owner->_fn.write_bit = pifSpiDevice_WriteRegBit8;

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
	if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
	}
}
