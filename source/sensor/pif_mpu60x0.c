#include "core/pif_log.h"
#include "sensor/pif_mpu60x0.h"

#include <math.h>


static BOOL _changeFsSel(PifImuSensor* p_imu_sensor, PifMpu60x0FsSel fs_sel)
{
	if (!p_imu_sensor) return FALSE;
	p_imu_sensor->_gyro_gain = 131.0 / (1 << fs_sel);
	return TRUE;
}

static BOOL _changeAfsSel(PifImuSensor* p_imu_sensor, PifMpu60x0AfsSel afs_sel)
{
	if (!p_imu_sensor) return FALSE;
	p_imu_sensor->_accel_gain = 16384 >> afs_sel;
	return TRUE;
}

BOOL pifMpu60x0_Init(PifMpu60x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, PifImuSensor* p_imu_sensor)
{
#ifndef __PIF_NO_LOG__	
	const char ident[] = "MPU60X0 Ident: ";
#endif	
	uint8_t data;
	PifMpu60x0PwrMgmt1 pwr_mgmt_1;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu60x0));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_p_i2c->addr = addr;

	if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, MPU60X0_REG_WHO_AM_I, &data)) goto fail;
	if (data != addr) {
		pif_error = E_INVALID_ID;
		goto fail;
	}
#ifndef __PIF_NO_LOG__	
	if (data < 32) {
		pifLog_Printf(LT_INFO, "%s%Xh", ident, data >> 1);
	}
	else {
		pifLog_Printf(LT_INFO, "%s%c", ident, data >> 1);
	}
#endif

   	pwr_mgmt_1.byte = 0;
	pwr_mgmt_1.bit.device_reset = TRUE;
	if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, MPU60X0_REG_PWR_MGMT_1, pwr_mgmt_1.byte)) goto fail;
	pifTaskManager_YieldMs(100);

    if (!pifI2cDevice_ReadRegBit8(p_owner->_p_i2c, MPU60X0_REG_GYRO_CONFIG, MPU60X0_GYRO_CONFIG_FS_SEL, &data)) goto fail;
    if (!_changeFsSel(p_imu_sensor, data)) goto fail;

    if (!pifI2cDevice_ReadRegBit8(p_owner->_p_i2c, MPU60X0_REG_ACCEL_CONFIG, MPU60X0_ACCEL_CONFIG_AFS_SEL, &data)) goto fail;
    if (!_changeAfsSel(p_imu_sensor, data)) goto fail;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->temp_scale = 1;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_GYROSCOPE | IMU_MEASURE_ACCELERO;

	p_imu_sensor->__gyro_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__gyro_info.read = (PifImuSensorRead)pifMpu60x0_ReadGyro;
	p_imu_sensor->__gyro_info.p_issuer = p_owner;

	p_imu_sensor->__accel_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__accel_info.read = (PifImuSensorRead)pifMpu60x0_ReadAccel;
	p_imu_sensor->__accel_info.p_issuer = p_owner;

    // Reset calibrate values
    p_imu_sensor->__delta_gyro[AXIS_X] = 0;
    p_imu_sensor->__delta_gyro[AXIS_Y] = 0;
    p_imu_sensor->__delta_gyro[AXIS_Z] = 0;
    p_imu_sensor->__use_calibrate = FALSE;

    // Reset threshold values
    p_imu_sensor->__threshold_gyro[AXIS_X] = 0;
    p_imu_sensor->__threshold_gyro[AXIS_Y] = 0;
    p_imu_sensor->__threshold_gyro[AXIS_Z] = 0;
    p_imu_sensor->__actual_threshold = 0;
    return TRUE;

fail:
	pifMpu60x0_Clear(p_owner);
	return FALSE;
}

void pifMpu60x0_Clear(PifMpu60x0* p_owner)
{
    if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
    }
}

BOOL pifMpu60x0_SetGyroConfig(PifMpu60x0* p_owner, PifMpu60x0GyroConfig gyro_config)
{
    if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, MPU60X0_REG_GYRO_CONFIG, gyro_config.byte)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, gyro_config.bit.fs_sel);
	return TRUE;
}

BOOL pifMpu60x0_SetFsSel(PifMpu60x0* p_owner, PifMpu60x0FsSel fs_sel)
{
    if (!pifI2cDevice_WriteRegBit8(p_owner->_p_i2c, MPU60X0_REG_GYRO_CONFIG, MPU60X0_GYRO_CONFIG_FS_SEL, fs_sel)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, fs_sel);
	return TRUE;
}

BOOL pifMpu60x0_SetAccelConfig(PifMpu60x0* p_owner, PifMpu60x0AccelConfig accel_config)
{
    if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, MPU60X0_REG_ACCEL_CONFIG, accel_config.byte)) return FALSE;
    _changeAfsSel(p_owner->__p_imu_sensor, accel_config.bit.afs_sel);
	return TRUE;
}

BOOL pifMpu60x0_SetAfsSel(PifMpu60x0* p_owner, PifMpu60x0AfsSel afs_sel)
{
    if (!pifI2cDevice_WriteRegBit8(p_owner->_p_i2c, MPU60X0_REG_ACCEL_CONFIG, MPU60X0_ACCEL_CONFIG_AFS_SEL, afs_sel)) return FALSE;
    _changeAfsSel(p_owner->__p_imu_sensor, afs_sel);
	return TRUE;
}

BOOL pifMpu60x0_ReadGyro(PifMpu60x0* p_owner, int16_t* p_gyro)
{
	uint8_t data[6];

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MPU60X0_REG_GYRO_XOUT_H, data, 6)) return FALSE;

	p_gyro[AXIS_X] = (data[0] << 8) + data[1];
	p_gyro[AXIS_Y] = (data[2] << 8) + data[3];
	p_gyro[AXIS_Z] = (data[4] << 8) + data[5];
	if (p_owner->gyro_scale > 0) {
		p_gyro[AXIS_X] /= p_owner->gyro_scale;
		p_gyro[AXIS_Y] /= p_owner->gyro_scale;
		p_gyro[AXIS_Z] /= p_owner->gyro_scale;
	}
	return TRUE;
}

BOOL pifMpu60x0_ReadAccel(PifMpu60x0* p_owner, int16_t* p_accel)
{
	uint8_t data[6];

    if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MPU60X0_REG_ACCEL_XOUT_H, data, 6)) return FALSE;

	p_accel[AXIS_X] = (data[0] << 8) + data[1];
	p_accel[AXIS_Y] = (data[2] << 8) + data[3];
	p_accel[AXIS_Z] = (data[4] << 8) + data[5];
	if (p_owner->accel_scale > 0) {
		p_accel[AXIS_X] /= p_owner->accel_scale;
		p_accel[AXIS_Y] /= p_owner->accel_scale;
		p_accel[AXIS_Z] /= p_owner->accel_scale;
	}
	return TRUE;
}

BOOL pifMpu60x0_ReadTemperature(PifMpu60x0* p_owner, int16_t* p_temperature)
{
	uint8_t data[2];

    if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MPU60X0_REG_TEMP_OUT_H, data, 2)) return FALSE;
    *p_temperature = ((int16_t)((data[0] << 8) + data[1]) / 340.0 + 36.53) * p_owner->temp_scale;
	return TRUE;
}

BOOL pifMpu60x0_CalibrationGyro(PifMpu60x0* p_owner, uint8_t samples)
{
	int16_t data[3];
    float sumX = 0;
    float sumY = 0;
    float sumZ = 0;
    float sigmaX = 0;
    float sigmaY = 0;
    float sigmaZ = 0;
	PifImuSensor* p_imu_sensor = p_owner->__p_imu_sensor;

    // Read n-samples
    for (uint8_t i = 0; i < samples; i++) {
    	if (!pifMpu60x0_ReadGyro(p_owner, data)) return FALSE;

		sumX += data[AXIS_X];
		sumY += data[AXIS_Y];
		sumZ += data[AXIS_Z];

		sigmaX += data[AXIS_X] * data[AXIS_X];
		sigmaY += data[AXIS_Y] * data[AXIS_Y];
		sigmaZ += data[AXIS_Z] * data[AXIS_Z];

		pifTaskManager_YieldMs(5);
    }

    // Calculate delta vectors
    p_imu_sensor->__delta_gyro[AXIS_X] = sumX / samples;
    p_imu_sensor->__delta_gyro[AXIS_Y] = sumY / samples;
    p_imu_sensor->__delta_gyro[AXIS_Z] = sumZ / samples;

    // Calculate threshold vectors
    p_imu_sensor->__threshold[AXIS_X] = sqrt((sigmaX / 50) - (p_imu_sensor->__delta_gyro[AXIS_X] * p_imu_sensor->__delta_gyro[AXIS_X]));
    p_imu_sensor->__threshold[AXIS_Y] = sqrt((sigmaY / 50) - (p_imu_sensor->__delta_gyro[AXIS_Y] * p_imu_sensor->__delta_gyro[AXIS_Y]));
    p_imu_sensor->__threshold[AXIS_Z] = sqrt((sigmaZ / 50) - (p_imu_sensor->__delta_gyro[AXIS_Z] * p_imu_sensor->__delta_gyro[AXIS_Z]));

    // Set calibrate
	p_imu_sensor->__use_calibrate = TRUE;

    // If already set threshold, recalculate threshold vectors
    if (p_imu_sensor->__actual_threshold > 0) {
    	pifMpu60x0_SetThreshold(p_owner, p_imu_sensor->__actual_threshold);
    }
    return TRUE;
}

BOOL pifMpu60x0_SetThreshold(PifMpu60x0* p_owner, uint8_t multiple)
{
	PifImuSensor* p_imu_sensor = p_owner->__p_imu_sensor;

	if (multiple > 0) {
		// If not calibrated, need calibrate
		if (!p_owner->__p_imu_sensor->__use_calibrate)
		{
			if (!pifMpu60x0_CalibrationGyro(p_owner, 50)) return FALSE;
		}

		// Calculate threshold vectors
		p_imu_sensor->__threshold_gyro[AXIS_X] = p_imu_sensor->__threshold[AXIS_X] * multiple;
		p_imu_sensor->__threshold_gyro[AXIS_Y] = p_imu_sensor->__threshold[AXIS_Y] * multiple;
		p_imu_sensor->__threshold_gyro[AXIS_Z] = p_imu_sensor->__threshold[AXIS_Z] * multiple;
	}
	else {
		// No threshold
		p_imu_sensor->__threshold_gyro[AXIS_X] = 0;
		p_imu_sensor->__threshold_gyro[AXIS_Y] = 0;
		p_imu_sensor->__threshold_gyro[AXIS_Z] = 0;
	}

	// Remember old threshold value
	p_imu_sensor->__actual_threshold = multiple;
	return TRUE;
}
