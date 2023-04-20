#include "core/pif_log.h"
#include "sensor/pif_hmc5883.h"


static void _changeGain(PifImuSensor* p_imu_sensor, PifHmc5883Gain gain)
{
	switch (gain) {
	case HMC5883_GAIN_0_88GA:	p_imu_sensor->_mag_gain = 1370; break;
	case HMC5883_GAIN_1_3GA:	p_imu_sensor->_mag_gain = 1090; break;
	case HMC5883_GAIN_1_9GA:	p_imu_sensor->_mag_gain = 820; break;
	case HMC5883_GAIN_2_5GA:	p_imu_sensor->_mag_gain = 660; break;
	case HMC5883_GAIN_4GA:		p_imu_sensor->_mag_gain = 440; break;
	case HMC5883_GAIN_4_7GA:	p_imu_sensor->_mag_gain = 390; break;
	case HMC5883_GAIN_5_6GA:	p_imu_sensor->_mag_gain = 330; break;
	case HMC5883_GAIN_8_1GA:	p_imu_sensor->_mag_gain = 230; break;
	}
}

BOOL pifHmc5883_Init(PifHmc5883* p_owner, PifId id, PifI2cPort* p_i2c, PifImuSensor* p_imu_sensor)
{
#ifndef __PIF_NO_LOG__	
	const char ident[] = "HMC5883 Ident: ";
#endif	
	uint8_t data[4];

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifHmc5883));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_p_i2c->addr = HMC5883_I2C_ADDR;

    if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, HMC5883_REG_IDENT_A, data, 3)) goto fail;
	if (data[0] != 'H') {
		pif_error = E_INVALID_ID;
		goto fail;
	}
#ifndef __PIF_NO_LOG__	
    if (data[0] < 32 || data[1] < 32 || data[2] < 32) {
    	pifLog_Printf(LT_INFO, "%s%2Xh %2Xh %2Xh", ident, data[0], data[1], data[2]);
    }
    else {
    	pifLog_Printf(LT_INFO, "%s%c%c%c", ident, data[0], data[1], data[2]);
    }
#endif

    if (!pifI2cDevice_ReadRegBit8(p_owner->_p_i2c, HMC5883_REG_CONFIG_B, HMC5883_CONFIG_B_GAIN, data)) goto fail;
    _changeGain(p_imu_sensor, (PifHmc5883Gain)data[0]);

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->scale[AXIS_X] = 1.0f;
	p_owner->scale[AXIS_Y] = 1.0f;
	p_owner->scale[AXIS_Z] = 1.0f;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_MAGNETO;

	p_imu_sensor->__mag_info.read = (PifImuSensorRead)pifHmc5883_ReadMag;
	p_imu_sensor->__mag_info.p_issuer = p_owner;
    return TRUE;

fail:
	pifHmc5883_Clear(p_owner);
	return FALSE;
}

void pifHmc5883_Clear(PifHmc5883* p_owner)
{
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}

BOOL pifHmc5883_SetGain(PifHmc5883* p_owner, PifHmc5883Gain gain)
{
    if (!pifI2cDevice_WriteRegBit8(p_owner->_p_i2c, HMC5883_REG_CONFIG_B, HMC5883_CONFIG_B_GAIN, gain)) return FALSE;
	_changeGain(p_owner->__p_imu_sensor, gain);
    return TRUE;
}

BOOL pifHmc5883_ReadMag(PifHmc5883* p_owner, int16_t* p_mag)
{
	uint8_t data[6];

	if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, HMC5883_REG_STATUS, data)) return FALSE;
	if (!(data[0] & 1)) return FALSE;

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, HMC5883_REG_OUT_X_M, data, 6)) return FALSE;

	p_mag[AXIS_X] = (int16_t)((data[0] << 8) + data[1]) * p_owner->scale[AXIS_X];
	p_mag[AXIS_Z] = (int16_t)((data[2] << 8) + data[3]) * p_owner->scale[AXIS_Z];
	p_mag[AXIS_Y] = (int16_t)((data[4] << 8) + data[5]) * p_owner->scale[AXIS_Y];
	return TRUE;
}
