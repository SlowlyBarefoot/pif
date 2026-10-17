// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "sensor/pif_bmi270.h"

#include <math.h>


// BMI270 configuration file (feature engine microcode), "maximum FIFO" variant, from the
// Bosch Sensortec BMI270 Sensor API (https://github.com/boschsensortec/BMI270_SensorAPI).
// Copyright (c) 2020 Bosch Sensortec GmbH. All rights reserved. BSD-3-Clause; see
// THIRD_PARTY_NOTICES.md for the full license text.
/*! @name  Global array that stores the configuration file of BMI270 */
const uint8_t maximum_fifo_config_file[] = {
    0xc8, 0x2e, 0x00, 0x2e, 0x80, 0x2e, 0x1a, 0x00, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00,
    0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0x90, 0x32, 0x21, 0x2e, 0x59, 0xf5,
    0x10, 0x30, 0x21, 0x2e, 0x6a, 0xf5, 0x1a, 0x24, 0x22, 0x00, 0x80, 0x2e, 0x3b, 0x00, 0xc8, 0x2e, 0x44, 0x47, 0x22,
    0x00, 0x37, 0x00, 0xa4, 0x00, 0xff, 0x0f, 0xd1, 0x00, 0x07, 0xad, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1,
    0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00,
    0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x24, 0xfc, 0xf5, 0x80, 0x30, 0x40, 0x42, 0x50, 0x50, 0x00, 0x30, 0x12, 0x24, 0xeb,
    0x00, 0x03, 0x30, 0x00, 0x2e, 0xc1, 0x86, 0x5a, 0x0e, 0xfb, 0x2f, 0x21, 0x2e, 0xfc, 0xf5, 0x13, 0x24, 0x63, 0xf5,
    0xe0, 0x3c, 0x48, 0x00, 0x22, 0x30, 0xf7, 0x80, 0xc2, 0x42, 0xe1, 0x7f, 0x3a, 0x25, 0xfc, 0x86, 0xf0, 0x7f, 0x41,
    0x33, 0x98, 0x2e, 0xc2, 0xc4, 0xd6, 0x6f, 0xf1, 0x30, 0xf1, 0x08, 0xc4, 0x6f, 0x11, 0x24, 0xff, 0x03, 0x12, 0x24,
    0x00, 0xfc, 0x61, 0x09, 0xa2, 0x08, 0x36, 0xbe, 0x2a, 0xb9, 0x13, 0x24, 0x38, 0x00, 0x64, 0xbb, 0xd1, 0xbe, 0x94,
    0x0a, 0x71, 0x08, 0xd5, 0x42, 0x21, 0xbd, 0x91, 0xbc, 0xd2, 0x42, 0xc1, 0x42, 0x00, 0xb2, 0xfe, 0x82, 0x05, 0x2f,
    0x50, 0x30, 0x21, 0x2e, 0x21, 0xf2, 0x00, 0x2e, 0x00, 0x2e, 0xd0, 0x2e, 0xf0, 0x6f, 0x02, 0x30, 0x02, 0x42, 0x20,
    0x26, 0xe0, 0x6f, 0x02, 0x31, 0x03, 0x40, 0x9a, 0x0a, 0x02, 0x42, 0xf0, 0x37, 0x05, 0x2e, 0x5e, 0xf7, 0x10, 0x08,
    0x12, 0x24, 0x1e, 0xf2, 0x80, 0x42, 0x83, 0x84, 0xf1, 0x7f, 0x0a, 0x25, 0x13, 0x30, 0x83, 0x42, 0x3b, 0x82, 0xf0,
    0x6f, 0x00, 0x2e, 0x00, 0x2e, 0xd0, 0x2e, 0x12, 0x40, 0x52, 0x42, 0x00, 0x2e, 0x12, 0x40, 0x52, 0x42, 0x3e, 0x84,
    0x00, 0x40, 0x40, 0x42, 0x7e, 0x82, 0xe1, 0x7f, 0xf2, 0x7f, 0x98, 0x2e, 0x6a, 0xd6, 0x21, 0x30, 0x23, 0x2e, 0x61,
    0xf5, 0xeb, 0x2c, 0xe1, 0x6f
};


// Performance mode from the datasheet (section 4.4): the accelerometer at 100Hz with normal
// averaging and the high-performance filter, the gyroscope at 200Hz with high-performance noise
// and filter.
#define BMI270_DEFAULT_ACC_CONF		(BMI270_AC_ACC_ODR_100 | BMI270_AC_ACC_BWP_NORM_AVG4 | BMI270_AC_ACC_FILTER_PERF_HP)
#define BMI270_DEFAULT_GYR_CONF		(BMI270_GC_GYR_ODR_200 | BMI270_GC_GYR_BWP_NORM | BMI270_GC_GYR_NOISE_PERF_HP | BMI270_GC_GYR_FILTER_PERF_HP)

// How long the feature engine is given to report on the configuration file, in ms. The datasheet
// says it does within 20ms.
#define BMI270_INIT_TIMEOUT			30


/**
 * @fn _readRegs
 * @brief Reads registers, leaving out the dummy byte the BMI270 sends first on every SPI read.
 *        Every read of this driver goes through here, which is also why pifDeviceReg_ReadRegBit8()
 *        and pifDeviceReg_WriteRegBit8() are not used: their read would take the dummy byte for
 *        the register.
 * @param p_owner Pointer to the owner instance.
 * @param reg First register to read.
 * @param p_data Buffer that receives the data.
 * @param size Number of bytes to read, at most 7.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _readRegs(PifBmi270* p_owner, uint8_t reg, uint8_t* p_data, uint8_t size)
{
	uint8_t buffer[8];
	uint8_t i;

	if (!p_owner->__dummy_read) return pifDeviceReg_ReadRegBytes(p_owner->_p_device, reg, p_data, size);

	if (size >= sizeof(buffer)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, reg, buffer, size + 1)) return FALSE;
	for (i = 0; i < size; i++) p_data[i] = buffer[i + 1];
	return TRUE;
}

/**
 * @fn _changeGyroRange
 * @brief Sets the gyro gain for a GYR_RANGE setting: 16.384 LSB/dps at 2000dps, doubling with
 *        each halving of the range.
 * @param p_imu_sensor Pointer to imu sensor.
 * @param range GYR_RANGE setting.
 */
static void _changeGyroRange(PifImuSensor* p_imu_sensor, uint8_t range)
{
	if (p_imu_sensor) p_imu_sensor->_gyro_gain = 16.384f * (1 << range);
}

/**
 * @fn _changeAccelRange
 * @brief Sets the accelerometer gain for an ACC_RANGE setting: 16384 LSB/g at 2g, halving with
 *        each doubling of the range.
 * @param p_imu_sensor Pointer to imu sensor.
 * @param range ACC_RANGE setting.
 */
static void _changeAccelRange(PifImuSensor* p_imu_sensor, uint8_t range)
{
	if (p_imu_sensor) p_imu_sensor->_accel_gain = 16384 >> range;
}

BOOL pifBmi270_Config(PifBmi270* p_owner, PifId id, PifImuSensor* p_imu_sensor)
{
	uint8_t data;

	if (!p_owner || !p_imu_sensor || !p_owner->_p_device) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	// The waits here hold the CPU on purpose: this runs once at start-up.
    // CMD 0xB6 resets the chip; every register then holds its power-on value. The Bosch Sensor API
    // leaves it 2ms before touching it again.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_CMD, BMI270_C_CMD_SOFT_RESET)) return FALSE;
	pif_Delay1ms(2);

	// The reset puts the chip back on I2C, and it is the rising edge of CSB at the end of a read
	// that switches it to SPI again, so what that first read returns is not to be trusted.
	if (p_owner->__dummy_read) {
		if (!_readRegs(p_owner, BMI270_REG_CHIP_ID, &data, 1)) return FALSE;
	}

	if (!_readRegs(p_owner, BMI270_REG_CHIP_ID, &data, 1)) return FALSE;
	if (data != BMI270_WHO_AM_I_CONST) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	if (!pifBmi270_UploadConfig(p_owner)) return FALSE;

	// Both sensors and the temperature sensor are off after the reset, and the readings stay 0
	// until they are turned on.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_PWR_CTRL,
			BMI270_PC_GYR_EN(1) | BMI270_PC_ACC_EN(1) | BMI270_PC_TEMP_EN(1))) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_ACC_CONF, BMI270_DEFAULT_ACC_CONF)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_GYR_CONF, BMI270_DEFAULT_GYR_CONF)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_PWR_CONF, BMI270_PC_FIFO_SELF_WAKE_UP(1))) return FALSE;

	if (!_readRegs(p_owner, BMI270_REG_ACC_RANGE, &data, 1)) return FALSE;
	_changeAccelRange(p_imu_sensor, data & BMI270_AR_ACC_RANGE_MASK);
	if (!_readRegs(p_owner, BMI270_REG_GYR_RANGE, &data, 1)) return FALSE;
	_changeGyroRange(p_imu_sensor, data & BMI270_GR_GYR_RANGE_MASK);

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->temp_scale = 1;
	p_owner->__p_imu_sensor = p_imu_sensor;

	p_imu_sensor->_measure |= IMU_MEASURE_GYROSCOPE | IMU_MEASURE_ACCELERO;

	p_imu_sensor->__gyro_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__gyro_info.read = (PifImuSensorRead)pifBmi270_ReadGyro;
	p_imu_sensor->__gyro_info.p_issuer = p_owner;

	p_imu_sensor->__accel_info.align = IMUS_ALIGN_CW0_DEG;
	p_imu_sensor->__accel_info.read = (PifImuSensorRead)pifBmi270_ReadAccel;
	p_imu_sensor->__accel_info.p_issuer = p_owner;

    pifImuSensor_ResetGyroCalibration(p_imu_sensor);
    return TRUE;
}

BOOL pifBmi270_UploadConfig(PifBmi270 *p_owner)
{
	uint16_t max_size = p_owner->_p_device->max_transfer_size;
	uint16_t chunk, len, pos;
	uint8_t addr[2];
	uint8_t data;
	int i;

	// INIT_DATA does not move on with the register address, so a file sent in pieces has to say
	// where each one goes, in words, through INIT_ADDR. Each piece is an even number of bytes.
	chunk = max_size ? (max_size & ~1U) : sizeof(maximum_fifo_config_file);
	if (!chunk) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// Advanced power save has to be off while the file goes in, and it wants 450us to get there.
    if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_PWR_CONF, 0)) return FALSE;
	pif_Delay1ms(1);
    if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_INIT_CTRL, 0)) return FALSE;

	for (pos = 0; pos < sizeof(maximum_fifo_config_file); pos += len) {
		len = sizeof(maximum_fifo_config_file) - pos;
		if (len > chunk) len = chunk;
		addr[0] = (pos / 2) & 0x0F;
		addr[1] = (pos / 2) >> 4;
		if (!pifDeviceReg_WriteRegBytes(p_owner->_p_device, BMI270_REG_INIT_ADDR_LSB, addr, 2)) return FALSE;
		if (!pifDeviceReg_WriteRegBytes(p_owner->_p_device, BMI270_REG_INIT_DATA, (uint8_t *)maximum_fifo_config_file + pos, len)) return FALSE;
	}

	// INIT_CTRL = 1 starts the feature engine on the file, and INTERNAL_STATUS says whether it
	// took it.
    if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_INIT_CTRL, 1)) return FALSE;
	for (i = 0; i < BMI270_INIT_TIMEOUT; i++) {
		pif_Delay1ms(1);
		if (!_readRegs(p_owner, BMI270_REG_INTERNAL_STATUS, &data, 1)) return FALSE;
		if ((data & BMI270_IS_MESSAGE_MASK) == BMI270_IS_MESSAGE_INIT_OK) return TRUE;
	}
	pif_error = E_INVALID_STATE;
	return FALSE;
}

BOOL pifBmi270_SetGyroRange(PifBmi270* p_owner, PifBmi270GrGyrRange range)
{
	uint8_t data;

	if (!p_owner || range > BMI270_GR_GYR_RANGE_125) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!_readRegs(p_owner, BMI270_REG_GYR_RANGE, &data, 1)) return FALSE;
	data = (data & ~BMI270_GR_GYR_RANGE_MASK) | range;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_GYR_RANGE, data)) return FALSE;
	_changeGyroRange(p_owner->__p_imu_sensor, range);
	return TRUE;
}

BOOL pifBmi270_SetAccelRange(PifBmi270* p_owner, PifBmi270ArAccRange range)
{
	uint8_t data;

	if (!p_owner || range > BMI270_AR_ACC_RANGE_16G) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!_readRegs(p_owner, BMI270_REG_ACC_RANGE, &data, 1)) return FALSE;
	data = (data & ~BMI270_AR_ACC_RANGE_MASK) | range;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, BMI270_REG_ACC_RANGE, data)) return FALSE;
	_changeAccelRange(p_owner->__p_imu_sensor, range);
	return TRUE;
}

BOOL pifBmi270_ReadGyro(PifBmi270* p_owner, int16_t* p_gyro)
{
	uint8_t data[6];

	if (!p_owner || !p_gyro) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!_readRegs(p_owner, BMI270_REG_GYR_X_LSB, data, 6)) return FALSE;

	p_gyro[AXIS_X] = (int16_t)(data[0] | (data[1] << 8));
	p_gyro[AXIS_Y] = (int16_t)(data[2] | (data[3] << 8));
	p_gyro[AXIS_Z] = (int16_t)(data[4] | (data[5] << 8));
	if (p_owner->gyro_scale > 0) {
		p_gyro[AXIS_X] /= p_owner->gyro_scale;
		p_gyro[AXIS_Y] /= p_owner->gyro_scale;
		p_gyro[AXIS_Z] /= p_owner->gyro_scale;
	}
	return TRUE;
}

BOOL pifBmi270_ReadAccel(PifBmi270* p_owner, int16_t* p_accel)
{
	uint8_t data[6];

	if (!p_owner || !p_accel) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    if (!_readRegs(p_owner, BMI270_REG_ACC_X_LSB, data, 6)) return FALSE;

	p_accel[AXIS_X] = (int16_t)(data[0] | (data[1] << 8));
	p_accel[AXIS_Y] = (int16_t)(data[2] | (data[3] << 8));
	p_accel[AXIS_Z] = (int16_t)(data[4] | (data[5] << 8));
	if (p_owner->accel_scale > 0) {
		p_accel[AXIS_X] /= p_owner->accel_scale;
		p_accel[AXIS_Y] /= p_owner->accel_scale;
		p_accel[AXIS_Z] /= p_owner->accel_scale;
	}
	return TRUE;
}

BOOL pifBmi270_ReadTemperature(PifBmi270* p_owner, int16_t* p_temperature)
{
	uint8_t data[2];
	int16_t raw;

	if (!p_owner || !p_temperature) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    if (!_readRegs(p_owner, BMI270_REG_TEMPERATURE_LSB, data, 2)) return FALSE;
	raw = (int16_t)(data[0] | (data[1] << 8));

	// 0x8000 is what the register holds while there is no valid temperature.
	if (raw == INT16_MIN) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	// 0 is 23 DegC, at 1/512 K per LSB.
    *p_temperature = (23.0f + raw / 512.0f) * p_owner->temp_scale;
	return TRUE;
}
