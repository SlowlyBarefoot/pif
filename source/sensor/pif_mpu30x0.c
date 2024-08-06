#include "core/pif_log.h"
#include "sensor/pif_mpu30x0.h"

#include <math.h>


static BOOL _changeFsSel(PifImuSensor* p_imu_sensor, PifMpu30x0FsSel fs_sel)
{
	if (!p_imu_sensor) return FALSE;
	p_imu_sensor->_gyro_gain = 131.0 / (1 << fs_sel);
	return TRUE;
}

BOOL pifMpu30x0_Detect(PifI2cPort* p_i2c, uint8_t addr)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "MPU30X0 Ident: ";
#endif	
	uint8_t data;
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, addr);

	if (!pifI2cDevice_ReadRegByte(p_device, MPU30X0_REG_WHO_AM_I, &data)) return FALSE;
	if (data != addr) return FALSE;
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

BOOL pifMpu30x0_Init(PifMpu30x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, PifImuSensor* p_imu_sensor)
{
	uint8_t data;
	BOOL change;
	PifMpu30x0PwrMgmt pwr_mgmt;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMpu30x0));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c, addr);
    if (!p_owner->_p_i2c) return FALSE;

    if (!pifI2cDevice_ReadRegBit8(p_owner->_p_i2c, MPU30X0_REG_DLPF_FS_SYNC, MPU30X0_DLPF_FS_SYNC_FS_SEL, &data)) goto fail;
    if (!_changeFsSel(p_imu_sensor, data)) goto fail;

    if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, MPU30X0_REG_PWR_MGMT, &pwr_mgmt.byte)) goto fail;
    change = FALSE;
    if (pwr_mgmt.bit.clk_sel != MPU30X0_CLK_SEL_PLL_XGYRO) {
    	pwr_mgmt.bit.clk_sel = MPU30X0_CLK_SEL_PLL_XGYRO;
        change = TRUE;
    }
    if (pwr_mgmt.bit.sleep == TRUE) {
    	pwr_mgmt.bit.sleep = FALSE;
        change = TRUE;
    }
    if (change) {
    	if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, MPU30X0_REG_PWR_MGMT, pwr_mgmt.byte)) goto fail;
    }

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_GYROSCOPE;

	p_imu_sensor->__gyro_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__gyro_info.read = (PifImuSensorRead)pifMpu30x0_ReadGyro;
	p_imu_sensor->__gyro_info.p_issuer = p_owner;

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
	pifMpu30x0_Clear(p_owner);
	return FALSE;
}

void pifMpu30x0_Clear(PifMpu30x0* p_owner)
{
    if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
    }
}

BOOL pifMpu30x0_SetDlpfFsSync(PifMpu30x0* p_owner, PifMpu30x0DlpfFsSync dlpf_fs_sync)
{
    if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, MPU30X0_REG_DLPF_FS_SYNC, dlpf_fs_sync.byte)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, dlpf_fs_sync.bit.fs_sel);
	return TRUE;
}

BOOL pifMpu30x0_SetFsSel(PifMpu30x0* p_owner, PifMpu30x0FsSel fs_sel)
{
    if (!pifI2cDevice_WriteRegBit8(p_owner->_p_i2c, MPU30X0_REG_DLPF_FS_SYNC, MPU30X0_DLPF_FS_SYNC_FS_SEL, fs_sel)) return FALSE;
    _changeFsSel(p_owner->__p_imu_sensor, fs_sel);
	return TRUE;
}

BOOL pifMpu30x0_ReadGyro(PifMpu30x0* p_owner, int16_t* p_gyro)
{
	uint8_t data[6];

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MPU30X0_REG_GYRO_XOUT_H, data, 6)) return FALSE;

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

    if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MPU30X0_REG_TEMP_OUT_H, data, 2)) return FALSE;
    *p_temperature = 35.0f + (((data[0] << 8) + data[1]) + 13200.0f) / 280.0f;
	return TRUE;
}

BOOL pifMpu30x0_CalibrationGyro(PifMpu30x0* p_owner, uint8_t samples)
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
    	if (!pifMpu30x0_ReadGyro(p_owner, data)) return FALSE;

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
    	pifMpu30x0_SetThreshold(p_owner, p_imu_sensor->__actual_threshold);
    }
    return TRUE;
}

BOOL pifMpu30x0_SetThreshold(PifMpu30x0* p_owner, uint8_t multiple)
{
	PifImuSensor* p_imu_sensor = p_owner->__p_imu_sensor;

	if (multiple > 0) {
		// If not calibrated, need calibrate
		if (!p_owner->__p_imu_sensor->__use_calibrate)
		{
			if (!pifMpu30x0_CalibrationGyro(p_owner, 50)) return FALSE;
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
