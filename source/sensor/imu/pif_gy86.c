// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/imu/pif_gy86.h"


// The six measurement registers of the magnetometer, read by the auxiliary I2C master of the MPU-6050 into
// EXT_SENS_DATA_00 and on.
#define GY86_MAG_LENGTH		6


/**
 * @fn _setupMpu6050
 * @brief Puts the gyroscope and the accelerometer of the module into the state the parameters ask for, and
 *        ties the auxiliary bus to the host bus so that the magnetometer on it can be reached.
 * @param p_owner Pointer to the owner instance.
 * @param p_param Settings of the sensors, or NULL for their defaults.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _setupMpu6050(PifGy86* p_owner, PifGy86Param* p_param)
{
	PifDeviceReg* p_device = p_owner->_mpu6050._p_device;

	// The sample rate of the gyroscope undivided, which is where the low pass filter leaves it.
	if (!pifDeviceReg_WriteRegByte(p_device, MPU60X0_REG_SMPLRT_DIV, 0)) return FALSE;

	// The gyroscope PLL is a better clock than the internal oscillator, and taking the device out of sleep
	// is the same write.
	if (!pifDeviceReg_WriteRegByte(p_device, MPU60X0_REG_PWR_MGMT_1,
			p_param ? p_param->mpu60x0_clksel : MPU60X0_CLKSEL_PLL_ZGYRO)) return FALSE;

	if (p_param) {
		if (!pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_CONFIG, MPU60X0_DLPF_CFG_MASK,
				p_param->mpu60x0_dlpf_cfg)) return FALSE;
		if (!pifMpu60x0_SetFsSel(&p_owner->_mpu6050, p_param->mpu60x0_fs_sel)) return FALSE;
		if (!pifMpu60x0_SetAfsSel(&p_owner->_mpu6050, p_param->mpu60x0_afs_sel)) return FALSE;
	}

	if (!pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_I2C_MST_CTRL, MPU60X0_I2C_MST_CLK_MASK,
			p_param ? p_param->mpu60x0_i2c_mst_clk : MPU60X0_I2C_MST_CLK_DEFAULT)) return FALSE;

	// An interrupt per sample, cleared by any read rather than only by reading INT_STATUS.
	if (!pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_INT_ENABLE, MPU60X0_DATA_RDY_EN_MASK,
			MPU60X0_DATA_RDY_EN(1))) return FALSE;

	// With the bypass on and the auxiliary master still off, the magnetometer answers on the host bus.
	return pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_INT_PIN_CFG,
			MPU60X0_INT_RD_CLEAR_MASK | MPU60X0_I2C_BYPASS_EN_MASK,
			MPU60X0_INT_RD_CLEAR(1) | MPU60X0_I2C_BYPASS_EN(1));
}

/**
 * @fn _setupHmc5883
 * @brief Sets up the magnetometer through the bypass, while it can still be reached directly.
 * @param p_owner Pointer to the owner instance.
 * @param p_i2c I2C port the device is on.
 * @param p_param Settings of the sensors, or NULL for their defaults.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _setupHmc5883(PifGy86* p_owner, PifI2cPort* p_i2c, PifGy86Param* p_param, PifImuSensor* p_imu_sensor)
{
	if (!pifHmc5883_Detect(p_i2c, NULL)) return FALSE;

	// The self test of the magnetometer runs in here, and leaves it measuring continuously.
	if (!pifHmc5883_Init(&p_owner->_hmc5883, PIF_ID_AUTO, p_i2c, NULL, p_imu_sensor)) return FALSE;

	if (p_param) {
		if (!pifDeviceReg_WriteRegByte(p_owner->_hmc5883._p_device, HMC5883_REG_CONFIG_A,
				(uint8_t)(HMC5883_MEASURE_MODE_NORMAL | p_param->hmc5883_samples |
					p_param->hmc5883_data_rate))) return FALSE;
		if (!pifHmc5883_SetGain(&p_owner->_hmc5883, p_param->hmc5883_gain)) return FALSE;
		if (!pifDeviceReg_WriteRegBit8(p_owner->_hmc5883._p_device, HMC5883_REG_MODE, HMC5883_MODE_MASK,
				p_param->hmc5883_mode)) return FALSE;
	}
	return TRUE;
}

/**
 * @fn _setupMagMaster
 * @brief Takes the bypass away and leaves the MPU-6050 master of its auxiliary bus, reading the measurement
 *        registers of the magnetometer into its own external sensor registers once per sample.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _setupMagMaster(PifGy86* p_owner)
{
	PifDeviceReg* p_device = p_owner->_mpu6050._p_device;

	if (!pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_INT_PIN_CFG, MPU60X0_I2C_BYPASS_EN_MASK,
			MPU60X0_I2C_BYPASS_EN(0))) return FALSE;

	if (!pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_USER_CTRL, MPU60X0_I2C_MST_EN_MASK,
			MPU60X0_I2C_MST_EN(1))) return FALSE;

	// Slave 0 reads the magnetometer: its address with the read bit, the register the run starts at, and
	// the number of bytes the run takes.
	if (!pifDeviceReg_WriteRegByte(p_device, MPU60X0_REG_I2C_SLV0_ADDR,
			MPU60X0_I2C_SLV_ADDR(HMC5883_I2C_ADDR) | MPU60X0_I2C_SLV_RW(1))) return FALSE;

	if (!pifDeviceReg_WriteRegByte(p_device, MPU60X0_REG_I2C_SLV0_REG, HMC5883_REG_OUT_X_M)) return FALSE;

	return pifDeviceReg_WriteRegBit8(p_device, MPU60X0_REG_I2C_SLV0_CTRL,
			MPU60X0_I2C_SLV_LEN_MASK | MPU60X0_I2C_SLV_EN_MASK,
			MPU60X0_I2C_SLV_LEN(GY86_MAG_LENGTH) | MPU60X0_I2C_SLV_EN(1));
}

BOOL pifGy86_Detect(PifI2cPort* p_i2c)
{
    return pifMpu60x0I2c_Detect(p_i2c, MPU60X0_I2C_ADDR(0), NULL);
}

BOOL pifGy86_Init(PifGy86* p_owner, PifId id, PifI2cPort* p_i2c, PifGy86Param* p_param, PifImuSensor* p_imu_sensor)
{
	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGy86));

    if (!pifMpu60x0I2c_Init(&p_owner->_mpu6050, PIF_ID_AUTO, p_i2c, MPU60X0_I2C_ADDR(0), NULL, p_imu_sensor)) goto fail;

    if (!_setupMpu6050(p_owner, p_param)) goto fail;

    pif_Delay1ms(PIF_GY86_BYPASS_DELAY_MS);

    if (!_setupHmc5883(p_owner, p_i2c, p_param, p_imu_sensor)) goto fail;

    if (!_setupMagMaster(p_owner)) goto fail;

    if (p_param && p_param->ms5611_evt_read) {
    	if (!pifMs5611_Init(&p_owner->_ms5611, PIF_ID_AUTO, p_i2c, MS5611_I2C_ADDR(1), NULL)) goto fail;

        pifMs5611_SetOverSamplingRate(&p_owner->_ms5611, p_param->ms5611_osr);

        if (!pifMs5611_AttachTaskForReading(&p_owner->_ms5611, PIF_ID_AUTO, p_param->ms5611_read_period,
        		p_param->ms5611_evt_read, FALSE, NULL)) goto fail;
    }

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

	// The magnetometer is read through the MPU-6050 from here on, so the IMU sensor gets this driver in
	// place of the one pifHmc5883_Init() registered.
	p_imu_sensor->__mag_info.read = (PifImuSensorRead)pifGy86_ReadMag;
	p_imu_sensor->__mag_info.p_issuer = p_owner;
    return TRUE;

fail:
	pifGy86_Clear(p_owner);
	return FALSE;
}

void pifGy86_Clear(PifGy86* p_owner)
{
    pifMs5611_Clear(&p_owner->_ms5611);
    pifHmc5883_Clear(&p_owner->_hmc5883);
    pifMpu60x0I2c_Clear(&p_owner->_mpu6050);
}

BOOL pifGy86_ReadMag(PifGy86* p_owner, int16_t* p_mag)
{
	uint8_t data[GY86_MAG_LENGTH];
	int axis;

	if (!pifDeviceReg_ReadRegBytes(p_owner->_mpu6050._p_device, MPU60X0_REG_EXT_SENS_DATA_00, data,
			GY86_MAG_LENGTH)) return FALSE;

	// The bytes are the measurement registers of the magnetometer as they stand: each axis MSB first, and
	// Z between X and Y.
	p_mag[AXIS_X] = (int16_t)((data[0] << 8) + data[1]);
	p_mag[AXIS_Z] = (int16_t)((data[2] << 8) + data[3]);
	p_mag[AXIS_Y] = (int16_t)((data[4] << 8) + data[5]);

	for (axis = 0; axis < AXIS_COUNT; axis++) {
		p_mag[axis] = (int16_t)(p_mag[axis] * p_owner->_hmc5883.scale[axis]);
	}
	return TRUE;
}
