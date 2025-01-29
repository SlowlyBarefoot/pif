#include "core/pif_log.h"
#include "sensor/pif_gy86.h"


BOOL pifGy86_Detect(PifI2cPort* p_i2c)
{
    if (!pifMpu60x0_Detect(p_i2c, MPU60X0_I2C_ADDR(0))) return FALSE;
//    if (!pifHmc5883_Detect(p_i2c)) return FALSE;
    return TRUE;
}

BOOL pifGy86_Init(PifGy86* p_owner, PifId id, PifI2cPort* p_i2c, PifGy86Param* p_param, PifImuSensor* p_imu_sensor)
{
	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGy86));

    if (!pifMpu60x0_Init(&p_owner->_mpu6050, PIF_ID_AUTO, p_i2c, MPU60X0_I2C_ADDR(0), p_imu_sensor)) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_SMPLRT_DIV, 0)) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_PWR_MGMT_1,
    		p_param ? p_param->mpu60x0_clksel : MPU60X0_CLKSEL_PLL_ZGYRO)) goto fail;

    if (p_param) {
    	if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_CONFIG,
    			MPU60X0_DLPF_CFG_MASK, p_param->mpu60x0_dlpf_cfg)) goto fail;

    	if (!pifMpu60x0_SetFsSel(&p_owner->_mpu6050, p_param->mpu60x0_fs_sel)) goto fail;

        if (!pifMpu60x0_SetAfsSel(&p_owner->_mpu6050, p_param->mpu60x0_afs_sel)) goto fail;
    }

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_MST_CTRL,
    		MPU60X0_I2C_MST_CLK_MASK, p_param->mpu60x0_i2c_mst_clk)) goto fail;

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_PIN_CFG,
    		MPU60X0_INT_RD_CLEAR_MASK | MPU60X0_I2C_BYPASS_EN_MASK,
			MPU60X0_INT_RD_CLEAR(1) | MPU60X0_I2C_BYPASS_EN(1))) goto fail;

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_ENABLE,
    		MPU60X0_DATA_RDY_EN_MASK, MPU60X0_DATA_RDY_EN(1))) goto fail;

    pif_Delay1ms(100);

    if (!pifHmc5883_Detect(p_i2c)) goto fail;

    if (!pifHmc5883_Init(&p_owner->_hmc5883, PIF_ID_AUTO, p_i2c, p_imu_sensor)) goto fail;

    if (p_param) {
        if (!pifI2cDevice_WriteRegByte(p_owner->_hmc5883._p_i2c, HMC5883_REG_CONFIG_A,
        		HMC5883_MEASURE_MODE_NORMAL | p_param->hmc5883_samples | p_param->hmc5883_data_rate)) goto fail;

        if (!pifHmc5883_SetGain(&p_owner->_hmc5883, p_param->hmc5883_gain)) goto fail;

        if (!pifI2cDevice_WriteRegBit8(p_owner->_hmc5883._p_i2c, HMC5883_REG_MODE, HMC5883_MODE_MASK, p_param->hmc5883_mode)) goto fail;
    }

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_PIN_CFG, MPU60X0_I2C_BYPASS_EN_MASK, FALSE)) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_ENABLE, 0x01)) goto fail; // DATA_RDY_EN interrupt enable

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_USER_CTRL, MPU60X0_I2C_MST_EN_MASK, TRUE)) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_SLV0_ADDR,
    		MPU60X0_I2C_SLV_ADDR(HMC5883_I2C_ADDR) | MPU60X0_I2C_SLV_RW(1))) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_SLV0_REG, HMC5883_REG_OUT_X_M)) goto fail;

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_SLV0_CTRL,
    		MPU60X0_I2C_SLV_LEN_MASK | MPU60X0_I2C_SLV_EN_MASK,
			MPU60X0_I2C_SLV_LEN(6) | MPU60X0_I2C_SLV_EN(1))) goto fail;

    if (p_param && p_param->ms5611_evt_read) {
    	if (!pifMs5611_Init(&p_owner->_ms5611, PIF_ID_AUTO, p_i2c, MS5611_I2C_ADDR(1))) goto fail;

        pifMs5611_SetOverSamplingRate(&p_owner->_ms5611, p_param->ms5611_osr);

        if (!pifMs5611_AddTaskForReading(&p_owner->_ms5611, p_param->ms5611_read_period, p_param->ms5611_evt_read, FALSE)) goto fail;
        p_owner->_ms5611._p_task->disallow_yield_id = p_param->disallow_yield_id;
    }

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__mag_start_reg = 0;

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
    pifMpu60x0_Clear(&p_owner->_mpu6050);
}

BOOL pifGy86_ReadMag(PifGy86* p_owner, int16_t* p_mag)
{
	uint8_t data[6];

	if (!pifI2cDevice_ReadRegBytes(p_owner->_mpu6050._p_i2c, MPU60X0_REG_EXT_SENS_DATA_00 + p_owner->__mag_start_reg, data, 6)) return FALSE;

	p_mag[AXIS_X] = (int16_t)((data[0] << 8) + data[1]) * p_owner->_hmc5883.scale[AXIS_X];
	p_mag[AXIS_Z] = (int16_t)((data[2] << 8) + data[3]) * p_owner->_hmc5883.scale[AXIS_Z];
	p_mag[AXIS_Y] = (int16_t)((data[4] << 8) + data[5]) * p_owner->_hmc5883.scale[AXIS_Y];
	return TRUE;
}
