#include "core/pif_log.h"
#include "sensor/pif_qmc5883.h"


static void _changeGain(PifImuSensor* p_imu_sensor, PifQmc5883Rng gain)
{
	switch (gain) {
	case QMC5883_RNG_2G:	p_imu_sensor->_mag_gain = 12000; break;
	case QMC5883_RNG_8G:	p_imu_sensor->_mag_gain = 3000; break;
	}
}

BOOL pifQmc5883_Init(PifQmc5883* p_owner, PifId id, PifI2cPort* p_i2c, PifImuSensor* p_imu_sensor)
{
	uint8_t data;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifQmc5883));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_p_i2c->addr = QMC5883_I2C_ADDR;

    if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, QMC5883_REG_CHIP_ID, &data, 1)) goto fail;
	if (data != 0xFF) {
		pif_error = E_INVALID_ID;
		goto fail;
	}

    if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, QMC5883_REG_SET_RESET_PERIOD, 1)) return FALSE;

    if (!pifI2cDevice_ReadRegBit8(p_owner->_p_i2c, QMC5883_REG_CONTROL_1, QMC5883_CONTROL_1_RNG, &data)) goto fail;
    _changeGain(p_imu_sensor, (PifQmc5883Rng)data);

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_MAGNETO;

	p_imu_sensor->__mag_info.read = (PifImuSensorRead)pifQmc5883_ReadMag;
	p_imu_sensor->__mag_info.p_issuer = p_owner;
    return TRUE;

fail:
	pifQmc5883_Clear(p_owner);
	return FALSE;
}

void pifQmc5883_Clear(PifQmc5883* p_owner)
{
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}

BOOL pifQmc5883_SetControl1(PifQmc5883* p_owner, PifQmc5883Control1 contorl_1)
{
    if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, QMC5883_REG_CONTROL_1, contorl_1.byte)) return FALSE;
	_changeGain(p_owner->__p_imu_sensor, contorl_1.bit.rng);
    return TRUE;
}

BOOL pifQmc5883_ReadMag(PifQmc5883* p_owner, int16_t* p_mag)
{
	PifQmc5883Status status;
	uint8_t data[6];

	if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, QMC5883_REG_STATUS, &status.byte)) return FALSE;
	if (!status.bit.drdy) return FALSE;

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, QMC5883_REG_OUT_X_LSB, data, 6)) return FALSE;

	p_mag[AXIS_X] = (int16_t)((data[1] << 8) + data[0]);
	p_mag[AXIS_Z] = (int16_t)((data[3] << 8) + data[2]);
	p_mag[AXIS_Y] = (int16_t)((data[5] << 8) + data[4]);
	return TRUE;
}
