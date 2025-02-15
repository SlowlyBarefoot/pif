#include "sensor/pif_bmi270_spi.h"


BOOL pifBmi270Spi_Detect(PifSpiPort *p_spi, void *p_client)
{
	uint8_t data[2];
	PifSpiDevice *p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi, p_client);

	if (!pifSpiDevice_ReadRegBytes(p_device, BMI270_REG_CHIP_ID | 0x80, data, 2)) return FALSE;
	if (data[1] != BMI270_WHO_AM_I_CONST) return FALSE;
	return TRUE;
}

BOOL pifBmi270Spi_Init(PifBmi270 *p_owner, PifId id, PifSpiPort *p_spi, void *p_client, PifImuSensor *p_imu_sensor)
{
	if (!p_owner || !p_spi || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmi270));

    p_owner->_p_spi = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO, p_client);
    if (!p_owner->_p_spi) return FALSE;

    p_owner->_fn.p_device = p_owner->_p_spi;

	p_owner->_fn.read_byte = pifSpiDevice_ReadRegByte;
	p_owner->_fn.read_bytes = pifSpiDevice_ReadRegBytes;
	p_owner->_fn.read_bit = pifSpiDevice_ReadRegBit8;

	p_owner->_fn.write_byte = pifSpiDevice_WriteRegByte;
	p_owner->_fn.write_bytes = pifSpiDevice_WriteRegBytes;
	p_owner->_fn.write_bit = pifSpiDevice_WriteRegBit8;

	if (!pifBmi270_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifBmi270Spi_Clear(p_owner);
	return FALSE;
}

void pifBmi270Spi_Clear(PifBmi270 *p_owner)
{
    if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
    }
}
