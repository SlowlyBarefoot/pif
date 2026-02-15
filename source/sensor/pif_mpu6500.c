#include "core/pif_log.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_mpu6500.h"

#include <math.h>


/**
 * @fn _changeFsSel
 * @brief Internal helper that supports change fs sel logic.
 * @param p_imu_sensor Pointer to imu sensor.
 * @param gyro_fs_sel Gyroscope full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _changeFsSel(PifImuSensor* p_imu_sensor, PifMpu6500GyroFsSel gyro_fs_sel)
{
	if (!p_imu_sensor) return FALSE;
	p_imu_sensor->_gyro_gain = 131.0 / (1 << (gyro_fs_sel >> 3));
	return TRUE;
}

/**
 * @fn _changeAccelFsSel
 * @brief Internal helper that supports change accel fs sel logic.
 * @param p_imu_sensor Pointer to imu sensor.
 * @param accel_fs_sel Accelerometer full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _changeAccelFsSel(PifImuSensor* p_imu_sensor, PifMpu6500AccelFsSel accel_fs_sel)
{
	if (!p_imu_sensor) return FALSE;
	p_imu_sensor->_accel_gain = 16384 >> (accel_fs_sel >> 3);
	return TRUE;
}

BOOL pifMpu6500_Config(PifMpu6500* p_owner, PifId id, PifImuSensor* p_imu_sensor)
{
	uint8_t data;
	BOOL change;

	if (!p_owner || !p_imu_sensor || !p_owner->_fn.p_device
			|| !p_owner->_fn.read_byte || !p_owner->_fn.read_bytes || !p_owner->_fn.read_bit
			|| !p_owner->_fn.write_byte || !p_owner->_fn.write_bytes || !p_owner->_fn.write_bit) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

    if (!(p_owner->_fn.read_bit)(p_owner->_fn.p_device, MPU6500_REG_GYRO_CONFIG, MPU6500_GYRO_FS_SEL_MASK, &data)) return FALSE;
    if (!_changeFsSel(p_imu_sensor, data)) return FALSE;

    if (!(p_owner->_fn.read_bit)(p_owner->_fn.p_device, MPU6500_REG_ACCEL_CONFIG, MPU6500_ACCEL_FS_SEL_MASK, &data)) return FALSE;
    if (!_changeAccelFsSel(p_imu_sensor, data)) return FALSE;

    if (!(p_owner->_fn.read_byte)(p_owner->_fn.p_device, MPU6500_REG_PWR_MGMT_1, &data)) return FALSE;
    change = FALSE;
    if (data & MPU6500_TEMP_DIS_MASK) {
    	RESET_BIT_FILED(data, MPU6500_TEMP_DIS_MASK);
        change = TRUE;
    }
    if (data & MPU6500_SLEEP_MASK) {
    	RESET_BIT_FILED(data, MPU6500_SLEEP_MASK);
        change = TRUE;
    }
    if (change) {
    	if (!(p_owner->_fn.write_byte)(p_owner->_fn.p_device, MPU6500_REG_PWR_MGMT_1, data)) return FALSE;
    }

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->temp_scale = 1;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_GYROSCOPE | IMU_MEASURE_ACCELERO;

	p_imu_sensor->__gyro_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__gyro_info.read = (PifImuSensorRead)pifMpu6500_ReadGyro;
	p_imu_sensor->__gyro_info.p_issuer = p_owner;

	p_imu_sensor->__accel_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__accel_info.read = (PifImuSensorRead)pifMpu6500_ReadAccel;
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
}

BOOL pifMpu6500_SetGyroConfig(PifMpu6500* p_owner, uint8_t gyro_config)
{
    if (!(p_owner->_fn.write_byte)(p_owner->_fn.p_device, MPU6500_REG_GYRO_CONFIG, gyro_config)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, gyro_config & MPU6500_GYRO_FS_SEL_MASK);
	return TRUE;
}

BOOL pifMpu6500_SetGyroFsSel(PifMpu6500* p_owner, PifMpu6500GyroFsSel gyro_fs_sel)
{
    if (!(p_owner->_fn.write_bit)(p_owner->_fn.p_device, MPU6500_REG_GYRO_CONFIG, MPU6500_GYRO_FS_SEL_MASK, gyro_fs_sel)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, gyro_fs_sel);
	return TRUE;
}

BOOL pifMpu6500_SetAccelConfig(PifMpu6500* p_owner, uint8_t accel_config)
{
    if (!(p_owner->_fn.write_byte)(p_owner->_fn.p_device, MPU6500_REG_ACCEL_CONFIG, accel_config)) return FALSE;
    _changeAccelFsSel(p_owner->__p_imu_sensor, accel_config & MPU6500_ACCEL_FS_SEL_MASK);
	return TRUE;
}

BOOL pifMpu6500_SetAccelFsSel(PifMpu6500* p_owner, PifMpu6500AccelFsSel accel_fs_sel)
{
    if (!(p_owner->_fn.write_bit)(p_owner->_fn.p_device, MPU6500_REG_ACCEL_CONFIG, MPU6500_ACCEL_FS_SEL_MASK, accel_fs_sel)) return FALSE;
    _changeAccelFsSel(p_owner->__p_imu_sensor, accel_fs_sel);
	return TRUE;
}

BOOL pifMpu6500_ReadGyro(PifMpu6500* p_owner, int16_t* p_gyro)
{
	uint8_t data[6];

	if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, MPU6500_REG_GYRO_XOUT_H, data, 6)) return FALSE;

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

BOOL pifMpu6500_ReadAccel(PifMpu6500* p_owner, int16_t* p_accel)
{
	uint8_t data[6];

    if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, MPU6500_REG_ACCEL_XOUT_H, data, 6)) return FALSE;

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

BOOL pifMpu6500_ReadTemperature(PifMpu6500* p_owner, int16_t* p_temperature)
{
	uint8_t data[2];
    const int16_t RoomTemp_Offset = 21;
    const int16_t Temp_Sensitivity = 333.87;

    if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, MPU6500_REG_TEMP_OUT_H, data, 2)) return FALSE;
    *p_temperature = (RoomTemp_Offset + ((int16_t)((data[0] << 8) | data[1]) - RoomTemp_Offset) / Temp_Sensitivity) * p_owner->temp_scale;
	return TRUE;
}

BOOL pifMpu6500_CalibrationGyro(PifMpu6500* p_owner, uint8_t samples)
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
    	if (!pifMpu6500_ReadGyro(p_owner, data)) return FALSE;

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
    	pifMpu6500_SetThreshold(p_owner, p_imu_sensor->__actual_threshold);
    }
    return TRUE;
}

BOOL pifMpu6500_SetThreshold(PifMpu6500* p_owner, uint8_t multiple)
{
	PifImuSensor* p_imu_sensor = p_owner->__p_imu_sensor;

	if (multiple > 0) {
		// If not calibrated, need calibrate
		if (!p_owner->__p_imu_sensor->__use_calibrate)
		{
			if (!pifMpu6500_CalibrationGyro(p_owner, 50)) return FALSE;
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
