// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_mpu6500.h"


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

    pifImuSensor_ResetGyroCalibration(p_imu_sensor);
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

    if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, MPU6500_REG_TEMP_OUT_H, data, 2)) return FALSE;
    *p_temperature = (21 + (int16_t)((data[0] << 8) | data[1]) / 333.87f) * p_owner->temp_scale;
	return TRUE;
}

BOOL pifMpu6500_CalibrationGyro(PifMpu6500* p_owner, uint8_t samples)
{
	return pifImuSensor_CalibrateGyro(p_owner->__p_imu_sensor, samples, 5);
}

BOOL pifMpu6500_SetThreshold(PifMpu6500* p_owner, uint8_t multiple)
{
	PifImuSensor* p_imu_sensor = p_owner->__p_imu_sensor;

	// A deadband needs the noise level, so calibrate first if that was not done yet.
	if (multiple && !p_imu_sensor->__gyro_calibrated) {
		if (!pifImuSensor_CalibrateGyro(p_imu_sensor, 50, 5)) return FALSE;
	}
	return pifImuSensor_SetGyroDeadband(p_imu_sensor, multiple);
}
