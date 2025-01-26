#include "core/pif_log.h"
#include "sensor/pif_mpu6500_spi.h"


BOOL pifMpu6500Spi_Detect(PifSpiPort* p_spi)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU6500 Ident: ";
#endif	
	uint8_t data;
	PifI2cDevice* p_device;

    p_device = pifSpiPort_TemporaryDevice(p_spi);

	if (!pifSpiDevice_ReadRegByte(p_device, MPU6500_REG_WHO_AM_I, &data)) return FALSE;
	if (data != MPU6500_WHO_AM_I_CONST) return FALSE;
#ifndef PIF_NO_LOG	
	if (data < 32) {
		pifLog_Printf(LT_INFO, "%s%Xh", ident, data >> 1);
	}
	else {
		pifLog_Printf(LT_INFO, "%s%c", ident, data >> 1);
	}
#endif
	return TRUE;
}

BOOL pifMpu6500Spi_Init(PifMpu6500* p_owner, PifId id, PifSpiPort* p_spi, PifImuSensor* p_imu_sensor)
{
	if (!p_owner || !p_spi || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu6500));

    p_owner->_p_spi = pifSpiPort_AddDevice(p_spi, PIF_ID_AUTO);
    if (!p_owner->_p_spi) return FALSE;

    p_owner->_fn.p_device = p_owner->_p_spi;

	p_owner->_fn.read_byte = pifSpiDevice_ReadRegByte;
	p_owner->_fn.read_bytes = pifSpiDevice_ReadRegBytes;
	p_owner->_fn.read_bit = pifSpiDevice_ReadRegBit8;

	p_owner->_fn.write_byte = pifSpiDevice_WriteRegByte;
	p_owner->_fn.write_bytes = pifSpiDevice_WriteRegBytes;
	p_owner->_fn.write_bit = pifSpiDevice_WriteRegBit8;

	if (!pifMpu6500_Config(p_owner, id, p_imu_sensor)) goto fail;
    return TRUE;

fail:
	pifMpu6500Spi_Clear(p_owner);
	return FALSE;
}

void pifMpu6500Spi_Clear(PifMpu6500* p_owner)
{
    if (p_owner->_p_spi) {
		pifI2cPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
    }
}
