// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "sensor/mag/pif_hmc5883.h"

#include <math.h>


// The self test drives each axis with a known bias current and the datasheet gives what the axis then reads:
// 1.16 Gauss on X and Y, 1.08 Gauss on Z. How far a measurement is off that is the error of the axis.
#define HMC5883_SELF_TEST_X_GAUSS	1.16f
#define HMC5883_SELF_TEST_Y_GAUSS	1.16f
#define HMC5883_SELF_TEST_Z_GAUSS	1.08f

// The gain the self test runs at. The bias is the largest field the device ever sees, and this is the range
// that holds it with room to spare on every axis.
#define HMC5883_SELF_TEST_GAIN		HMC5883_GAIN_2_5GA

// Measurements per bias direction. The bias is noisy, so the axes are worked out from the sum of several.
#define HMC5883_SELF_TEST_SAMPLES	10

// What an axis reads when the field is past the range of the gain, instead of a measurement.
#define HMC5883_OVERFLOW			(-4096)


/**
 * @fn _gainLsb
 * @brief The sensitivity of a gain setting in LSb per Gauss, as the datasheet tabulates it.
 * @param gain Gain configuration value.
 * @return LSb per Gauss.
 */
static uint16_t _gainLsb(PifHmc5883Gain gain)
{
	switch (gain) {
	case HMC5883_GAIN_0_88GA:	return 1370;
	case HMC5883_GAIN_1_9GA:	return 820;
	case HMC5883_GAIN_2_5GA:	return 660;
	case HMC5883_GAIN_4GA:		return 440;
	case HMC5883_GAIN_4_7GA:	return 390;
	case HMC5883_GAIN_5_6GA:	return 330;
	case HMC5883_GAIN_8_1GA:	return 230;
	default:					return 1090;	// HMC5883_GAIN_1_3GA, the gain the device starts at
	}
}

/**
 * @fn _readRaw
 * @brief Reads the three axes as the device holds them, without the calibration of the axes.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _readRaw(PifHmc5883* p_owner, int16_t* p_mag)
{
	uint8_t data[6];

	if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, HMC5883_REG_OUT_X_M, data, 6)) return FALSE;

	// Six registers from OUT_X_M on, each axis MSB first, and Z between X and Y.
	p_mag[AXIS_X] = (int16_t)((data[0] << 8) + data[1]);
	p_mag[AXIS_Z] = (int16_t)((data[2] << 8) + data[3]);
	p_mag[AXIS_Y] = (int16_t)((data[4] << 8) + data[5]);
	return TRUE;
}

/**
 * @fn _measureOnce
 * @brief Asks for one measurement and reads it once the device reports it ready. Reading the axes is what
 *        clears that report, so the next measurement starts from a clean one.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when a transfer failed or the device did not measure in time.
 */
static BOOL _measureOnce(PifHmc5883* p_owner, int16_t* p_mag)
{
	uint32_t start_time = pif_cumulative_timer1ms;
	uint8_t status;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, HMC5883_REG_MODE, HMC5883_MODE_SINGLE)) return FALSE;

	while (TRUE) {
		if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, HMC5883_REG_STATUS, &status)) return FALSE;
		if (status & HMC5883_RDY_MASK) return _readRaw(p_owner, p_mag);
		if (PIF_CHECK_ELAPSE_TIME_1MS(start_time, PIF_HMC5883_MEASURE_TIMEOUT_MS)) {
			pif_error = E_TIMEOUT;
			return FALSE;
		}
	}
}

/**
 * @fn _sumBias
 * @brief Adds up HMC5883_SELF_TEST_SAMPLES measurements taken with one of the two bias currents.
 * @param p_owner Pointer to the owner instance.
 * @param measure_mode HMC5883_MEASURE_MODE_POS_BIAS or HMC5883_MEASURE_MODE_NEG_BIAS.
 * @param p_total Each axis adds its measurements into this, the negative bias with the sign turned round, so
 *        that the two directions together come to twice the level of the bias.
 * @return TRUE on success, FALSE when a transfer failed, the device did not measure, or an axis overflowed.
 */
static BOOL _sumBias(PifHmc5883* p_owner, PifHmc5883MeasureMode measure_mode, int32_t* p_total)
{
	int16_t mag[AXIS_COUNT];
	int i, axis;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, HMC5883_REG_CONFIG_A,
			(uint8_t)(measure_mode | HMC5883_SAMPLES_1 | HMC5883_DATARATE_15HZ))) return FALSE;

	for (i = 0; i < HMC5883_SELF_TEST_SAMPLES; i++) {
		if (!_measureOnce(p_owner, mag)) return FALSE;
		for (axis = 0; axis < AXIS_COUNT; axis++) {
			// A gain that cannot hold the bias makes the whole sum worthless, so there is no going on.
			if (mag[axis] == HMC5883_OVERFLOW) {
				pif_error = E_WRONG_DATA;
				return FALSE;
			}
			if (measure_mode == HMC5883_MEASURE_MODE_NEG_BIAS) p_total[axis] -= mag[axis];
			else p_total[axis] += mag[axis];
		}
	}
	return TRUE;
}

/**
 * @fn _calibrateAxes
 * @brief Works out the scale of each axis from the level its self test bias comes back at.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE when the scales were worked out, FALSE when the self test could not be carried out, which
 *         leaves them as they were.
 */
static BOOL _calibrateAxes(PifHmc5883* p_owner)
{
	const float gauss[AXIS_COUNT] = { HMC5883_SELF_TEST_X_GAUSS, HMC5883_SELF_TEST_Y_GAUSS,
			HMC5883_SELF_TEST_Z_GAUSS };
	int32_t total[AXIS_COUNT] = { 0, 0, 0 };
	int16_t mag[AXIS_COUNT];
	float expected;
	int axis;

	if (!pifHmc5883_SetGain(p_owner, HMC5883_SELF_TEST_GAIN)) return FALSE;
	// The measurement that is running when the gain changes keeps the old one, as the datasheet says, so one
	// measurement goes to waste before the counts below mean anything. It also clears a measurement that
	// the device may have been holding already.
	if (!_measureOnce(p_owner, mag)) return FALSE;

	if (!_sumBias(p_owner, HMC5883_MEASURE_MODE_POS_BIAS, total)) return FALSE;
	if (!_sumBias(p_owner, HMC5883_MEASURE_MODE_NEG_BIAS, total)) return FALSE;

	for (axis = 0; axis < AXIS_COUNT; axis++) {
		if (!total[axis]) continue;
		// What the axis should have come to over both directions, against what it did come to.
		expected = gauss[axis] * _gainLsb(HMC5883_SELF_TEST_GAIN) * 2 * HMC5883_SELF_TEST_SAMPLES;
		p_owner->scale[axis] = fabsf(expected / total[axis]);
	}
	return TRUE;
}

/**
 * @fn _selfTest
 * @brief Calibrates the axes and says how it went. A self test that could not be carried out is not a
 *        failure of the setup: the scales stay at 1 and the readings are then uncorrected.
 * @param p_owner Pointer to the owner instance.
 */
static void _selfTest(PifHmc5883* p_owner)
{
	if (_calibrateAxes(p_owner)) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "HMC5883 scale: X=%f Y=%f Z=%f", (double)p_owner->scale[AXIS_X],
				(double)p_owner->scale[AXIS_Y], (double)p_owner->scale[AXIS_Z]);
#endif
		return;
	}
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_WARN, "HMC5883 self test failed: %d", pif_error);
#endif
}

BOOL pifHmc5883_Detect(PifI2cPort* p_i2c, void *p_client)
{
	uint8_t data[3];
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, HMC5883_I2C_ADDR, p_client);

	// The three identification registers of the device read "H43".
    if (!pifDeviceReg_ReadRegBytes((PifDeviceReg *)p_device, HMC5883_REG_IDENT_A, data, 3)) return FALSE;
	if (data[0] != 'H' || data[1] != '4' || data[2] != '3') return FALSE;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "HMC5883 Ident: %c%c%c", data[0], data[1], data[2]);
#endif
    return TRUE;
}

BOOL pifHmc5883_Init(PifHmc5883* p_owner, PifId id, PifI2cPort* p_i2c, void *p_client, PifImuSensor* p_imu_sensor)
{
	PifI2cDevice *p_device;

	uint8_t gain;

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifHmc5883));

    p_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, HMC5883_I2C_ADDR, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->scale[AXIS_X] = 1.0f;
	p_owner->scale[AXIS_Y] = 1.0f;
	p_owner->scale[AXIS_Z] = 1.0f;
	p_owner->__p_imu_sensor = p_imu_sensor;

	// The gain the device is found at. The self test needs one of its own, and this one goes back afterwards.
    if (!pifDeviceReg_ReadRegBit8(p_owner->_p_device, HMC5883_REG_CONFIG_B, HMC5883_GAIN_MASK, &gain)) goto fail;

    _selfTest(p_owner);

    if (!pifHmc5883_SetGain(p_owner, (PifHmc5883Gain)gain)) goto fail;

    // Measuring the field itself again, and from here on without being asked each time.
    if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, HMC5883_REG_CONFIG_A,
    		HMC5883_MEASURE_MODE_NORMAL | HMC5883_SAMPLES_DEFAULT | HMC5883_DATARATE_DEFAULT)) goto fail;
    if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, HMC5883_REG_MODE, HMC5883_MODE_CONTINOUS)) goto fail;

	p_imu_sensor->_measure |= IMU_MEASURE_MAGNETO;

	p_imu_sensor->__mag_info.read = (PifImuSensorRead)pifHmc5883_ReadMag;
	p_imu_sensor->__mag_info.p_issuer = p_owner;
    return TRUE;

fail:
	pifHmc5883_Clear(p_owner);
	return FALSE;
}

void pifHmc5883_Clear(PifHmc5883* p_owner)
{
	if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
	}
}

BOOL pifHmc5883_SetGain(PifHmc5883* p_owner, PifHmc5883Gain gain)
{
    if (!pifDeviceReg_WriteRegBit8(p_owner->_p_device, HMC5883_REG_CONFIG_B, HMC5883_GAIN_MASK, gain)) return FALSE;
	p_owner->__p_imu_sensor->_mag_gain = _gainLsb(gain);
    return TRUE;
}

BOOL pifHmc5883_ReadMag(PifHmc5883* p_owner, int16_t* p_mag)
{
	uint8_t status;
	int axis;

	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, HMC5883_REG_STATUS, &status)) return FALSE;
	if (!(status & HMC5883_RDY_MASK)) return FALSE;

	if (!_readRaw(p_owner, p_mag)) return FALSE;

	for (axis = 0; axis < AXIS_COUNT; axis++) {
		p_mag[axis] = (int16_t)(p_mag[axis] * p_owner->scale[axis]);
	}
	return TRUE;
}
