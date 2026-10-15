// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_mpu30x0.h"


/**
 * @fn _changeFsSel
 * @brief Internal helper that supports change fs sel logic.
 * @param p_imu_sensor Pointer to imu sensor.
 * @param fs_sel Gyroscope full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _changeFsSel(PifImuSensor* p_imu_sensor, PifMpu30x0FsSel fs_sel)
{
	if (!p_imu_sensor) return FALSE;
	p_imu_sensor->_gyro_gain = 131.0 / (1 << (fs_sel >> 3));
	return TRUE;
}

BOOL pifMpu30x0_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU30X0 Ident: ";
#endif	
	uint8_t data;
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr, p_client);

	if (!pifDeviceReg_ReadRegByte((PifDeviceReg *)p_device, MPU30X0_REG_WHO_AM_I, &data)) return FALSE;
	if (data != addr) return FALSE;
#ifndef PIF_NO_LOG	
	if (data < 64) {
		pifLog_Printf(LT_INFO, "%s%Xh", ident, data >> 1);
	}
	else {
		pifLog_Printf(LT_INFO, "%s%c", ident, data >> 1);
	}
#endif
	return TRUE;
}

BOOL pifMpu30x0_Init(PifMpu30x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor)
{
	PifI2cDevice *p_device;

	uint8_t data;
	BOOL change;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu30x0));

    p_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

    if (!pifDeviceReg_ReadRegBit8(p_owner->_p_device, MPU30X0_REG_DLPF_FS_SYNC, MPU30X0_FS_SEL_MASK, &data)) goto fail;
    if (!_changeFsSel(p_imu_sensor, data)) goto fail;

    if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MPU30X0_REG_PWR_MGMT, &data)) goto fail;
    change = FALSE;
    if ((data & MPU30X0_CLK_SEL_MASK) != MPU30X0_CLK_SEL_PLL_XGYRO) {
    	SET_BIT_FILED(data, MPU30X0_CLK_SEL_MASK, MPU30X0_CLK_SEL_PLL_XGYRO);
        change = TRUE;
    }
    if (data & MPU30X0_SLEEP_MASK) {
    	RESET_BIT_FILED(data, MPU30X0_SLEEP_MASK);
        change = TRUE;
    }
    if (change) {
    	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU30X0_REG_PWR_MGMT, data)) goto fail;
    }

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_GYROSCOPE;

	p_imu_sensor->__gyro_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__gyro_info.read = (PifImuSensorRead)pifMpu30x0_ReadGyro;
	p_imu_sensor->__gyro_info.p_issuer = p_owner;

    pifImuSensor_ResetGyroCalibration(p_imu_sensor);
    return TRUE;

fail:
	pifMpu30x0_Clear(p_owner);
	return FALSE;
}

void pifMpu30x0_Clear(PifMpu30x0* p_owner)
{
    if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    }
}

BOOL pifMpu30x0_SetDlpfFsSync(PifMpu30x0* p_owner, uint8_t dlpf_fs_sync)
{
    if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MPU30X0_REG_DLPF_FS_SYNC, dlpf_fs_sync)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, dlpf_fs_sync & MPU30X0_FS_SEL_MASK);
	return TRUE;
}

BOOL pifMpu30x0_SetFsSel(PifMpu30x0* p_owner, PifMpu30x0FsSel fs_sel)
{
    if (!pifDeviceReg_WriteRegBit8(p_owner->_p_device, MPU30X0_REG_DLPF_FS_SYNC, MPU30X0_FS_SEL_MASK, fs_sel)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, fs_sel);
	return TRUE;
}

BOOL pifMpu30x0_ReadGyro(PifMpu30x0* p_owner, int16_t* p_gyro)
{
	uint8_t data[6];

	if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, MPU30X0_REG_GYRO_XOUT_H, data, 6)) return FALSE;

	p_gyro[AXIS_X] = (data[0] << 8) | data[1];
	p_gyro[AXIS_Y] = (data[2] << 8) | data[3];
	p_gyro[AXIS_Z] = (data[4] << 8) | data[5];
	if (p_owner->scale > 0) {
		p_gyro[AXIS_X] /= p_owner->scale;
		p_gyro[AXIS_Y] /= p_owner->scale;
		p_gyro[AXIS_Z] /= p_owner->scale;
	}
	return TRUE;
}

BOOL pifMpu30x0_ReadTemperature(PifMpu30x0* p_owner, float* p_temperature)
{
	uint8_t data[2];

    if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, MPU30X0_REG_TEMP_OUT_H, data, 2)) return FALSE;
    *p_temperature = 35.0f + ((int16_t)((data[0] << 8) + data[1]) + 13200.0f) / 280.0f;
	return TRUE;
}

BOOL pifMpu30x0_CalibrationGyro(PifMpu30x0* p_owner, uint8_t samples)
{
	return pifImuSensor_CalibrateGyro(p_owner->__p_imu_sensor, samples, 5);
}

BOOL pifMpu30x0_SetThreshold(PifMpu30x0* p_owner, uint8_t multiple)
{
	PifImuSensor* p_imu_sensor = p_owner->__p_imu_sensor;

	// A deadband needs the noise level, so calibrate first if that was not done yet.
	if (multiple && !p_imu_sensor->__gyro_calibrated) {
		if (!pifImuSensor_CalibrateGyro(p_imu_sensor, 50, 5)) return FALSE;
	}
	return pifImuSensor_SetGyroDeadband(p_imu_sensor, multiple);
}
