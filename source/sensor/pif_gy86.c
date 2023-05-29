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
    PifMpu60x0I2cMstCtrl i2c_mst_ctrl;
	PifMpu60x0I2cSlvAddr i2c_slv_addr;
	PifMpu60x0I2cSlvCtrl i2c_slv_ctrl;
	PifMpu60x0IntEnable int_enable;
	PifMpu60x0IntPinCfg int_pin_cfg;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGy86));

    if (!pifMpu60x0_Init(&p_owner->_mpu6050, PIF_ID_AUTO, p_i2c, MPU60X0_I2C_ADDR(0), &p_param->mpu60x0, p_imu_sensor)) goto fail;

    i2c_mst_ctrl.byte = 0;
    i2c_mst_ctrl.bit.i2c_mst_clk = p_param ? p_param->mpu60x0_i2c_mst_clk : MPU60X0_I2C_MST_CLK_400KHZ;
    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_MST_CTRL, i2c_mst_ctrl.byte)) goto fail;

    int_pin_cfg.byte = 0;
    int_pin_cfg.bit.int_rd_clear = TRUE;
    int_pin_cfg.bit.i2c_bypass_en = TRUE;
    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_PIN_CFG, int_pin_cfg.byte)) goto fail;

    int_enable.byte = 0;
    int_enable.bit.data_rdy_en = TRUE;
    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_ENABLE, int_enable.byte)) goto fail;

    pif_Delay1ms(10);

    if (!pifHmc5883_Detect(p_i2c)) goto fail;

    if (!pifHmc5883_Init(&p_owner->_hmc5883, PIF_ID_AUTO, p_i2c, &p_param->hmc5883, p_imu_sensor)) goto fail;

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_PIN_CFG, MPU60X0_INT_PIN_CFG_I2C_BYPASS_EN, FALSE)) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_INT_ENABLE, 0x01)) goto fail; // DATA_RDY_EN interrupt enable

    if (!pifI2cDevice_WriteRegBit8(p_owner->_mpu6050._p_i2c, MPU60X0_REG_USER_CTRL, MPU60X0_USER_CTRL_I2C_MST_EN, TRUE)) goto fail;

    i2c_slv_addr.byte = 0;
    i2c_slv_addr.bit.i2c_slv_addr = HMC5883_I2C_ADDR;
    i2c_slv_addr.bit.i2c_slv_rw = TRUE;
    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_SLV0_ADDR, i2c_slv_addr.byte)) goto fail;

    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_SLV0_REG, HMC5883_REG_OUT_X_M)) goto fail;

    i2c_slv_ctrl.byte = 0;
    i2c_slv_ctrl.bit.i2c_slv_len = 6;
    i2c_slv_ctrl.bit.i2c_slv_en = TRUE;
    if (!pifI2cDevice_WriteRegByte(p_owner->_mpu6050._p_i2c, MPU60X0_REG_I2C_SLV0_CTRL, i2c_slv_ctrl.byte)) goto fail;

    if (p_param && p_param->ms5611.evt_read) {
    	if (!pifMs5611_Init(&p_owner->_ms5611, PIF_ID_AUTO, p_i2c, MS5611_I2C_ADDR(1), &p_param->ms5611)) goto fail;
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
