#include "core/pif_log.h"
#include "sensor/pif_hmc5883.h"

#include <math.h>


#define HMC58X3_X_SELF_TEST_GAUSS (+1.16f)       // X axis level when bias current is applied.
#define HMC58X3_Y_SELF_TEST_GAUSS (+1.16f)       // Y axis level when bias current is applied.
#define HMC58X3_Z_SELF_TEST_GAUSS (+1.08f)       // Z axis level when bias current is applied.


static void _changeGain(PifImuSensor* p_imu_sensor, PifHmc5883Gain gain)
{
	switch (gain) {
	case HMC5883_GAIN_0_88GA:	p_imu_sensor->_mag_gain = 1370; break;
	case HMC5883_GAIN_1_3GA:	p_imu_sensor->_mag_gain = 1090; break;
	case HMC5883_GAIN_1_9GA:	p_imu_sensor->_mag_gain = 820; break;
	case HMC5883_GAIN_2_5GA:	p_imu_sensor->_mag_gain = 660; break;
	case HMC5883_GAIN_4GA:		p_imu_sensor->_mag_gain = 440; break;
	case HMC5883_GAIN_4_7GA:	p_imu_sensor->_mag_gain = 390; break;
	case HMC5883_GAIN_5_6GA:	p_imu_sensor->_mag_gain = 330; break;
	case HMC5883_GAIN_8_1GA:	p_imu_sensor->_mag_gain = 230; break;
	}
}

BOOL pifHmc5883_Detect(PifI2cPort* p_i2c)
{
#ifndef PIF_NO_LOG	
	const char ident[] = "HMC5883 Ident: ";
#endif	
	uint8_t data[3];
	PifI2cDevice* p_device;

    p_device = pifI2cPort_TemporaryDevice(p_i2c, HMC5883_I2C_ADDR);

    if (!pifI2cDevice_ReadRegBytes(p_device, HMC5883_REG_IDENT_A, data, 3)) return FALSE;
	if (data[0] != 'H') return FALSE;
#ifndef PIF_NO_LOG	
    if (data[0] < 32 || data[1] < 32 || data[2] < 32) {
    	pifLog_Printf(LT_INFO, "%s%2Xh %2Xh %2Xh", ident, data[0], data[1], data[2]);
    }
    else {
    	pifLog_Printf(LT_INFO, "%s%c%c%c", ident, data[0], data[1], data[2]);
    }
#endif
    return TRUE;
}

BOOL pifHmc5883_Init(PifHmc5883* p_owner, PifId id, PifI2cPort* p_i2c, PifImuSensor* p_imu_sensor)
{
	uint8_t data[4];
	PifHmc5883ConfigA config_a;
    int16_t adc[3];
    int i;
    int32_t xyz_total[3] = { 0, 0, 0 }; // 32 bit totals so they won't overflow.
    BOOL bret = TRUE;           // Error indicator

	if (!p_owner || !p_i2c || !p_imu_sensor) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifHmc5883));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c, HMC5883_I2C_ADDR);
    if (!p_owner->_p_i2c) return FALSE;

    if (!pifI2cDevice_ReadRegBit8(p_owner->_p_i2c, HMC5883_REG_CONFIG_B, HMC5883_CONFIG_B_GAIN, data)) goto fail;
    _changeGain(p_imu_sensor, (PifHmc5883Gain)data[0]);

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->scale[AXIS_X] = 1.0f;
	p_owner->scale[AXIS_Y] = 1.0f;
	p_owner->scale[AXIS_Z] = 1.0f;
	p_owner->__p_imu_sensor = p_imu_sensor;

    config_a.byte = 0;
    config_a.bit.measure_mode = HMC5883_MEASURE_MODE_POS_BIAS;
    config_a.bit.data_rate = HMC5883_DATARATE_15HZ;
    pifI2cDevice_WriteRegByte(p_owner->_p_i2c, HMC5883_REG_CONFIG_A, config_a.byte);   // Reg A DOR = 0x010 + MS1, MS0 set to pos bias
    // Note that the  very first measurement after a gain change maintains the same gain as the previous setting.
    // The new gain setting is effective from the second measurement and on.
    pifHmc5883_SetGain(p_owner, HMC5883_GAIN_2_5GA); // Set the Gain to 2.5Ga (7:5->011)
    pif_Delay1ms(100);

    for (i = 0; i < 10;) {  // Collect 10 samples
        pifI2cDevice_WriteRegByte(p_owner->_p_i2c, HMC5883_REG_MODE, HMC5883_MODE_SINGLE);
        pif_Delay1ms(50);
        if (pifHmc5883_ReadMag(p_owner, adc)) {       // Get the raw values in case the scales have already been changed.
			// Since the measurements are noisy, they should be averaged rather than taking the max.
			xyz_total[AXIS_X] += adc[AXIS_X];
			xyz_total[AXIS_Y] += adc[AXIS_Y];
			xyz_total[AXIS_Z] += adc[AXIS_Z];

			// Detect saturation.
			if (-4096 >= MIN(adc[AXIS_X], MIN(adc[AXIS_Y], adc[AXIS_Z]))) {
				bret = FALSE;
				break;              // Breaks out of the for loop.  No sense in continuing if we saturated.
			}
			i++;
        }
    }

    // Apply the negative bias. (Same gain)
    config_a.bit.measure_mode = HMC5883_MEASURE_MODE_NEG_BIAS;
    pifI2cDevice_WriteRegByte(p_owner->_p_i2c, HMC5883_REG_CONFIG_A, config_a.byte);   // Reg A DOR = 0x010 + MS1, MS0 set to negative bias.
    for (i = 0; i < 10;) {
        pifI2cDevice_WriteRegByte(p_owner->_p_i2c, HMC5883_REG_MODE, HMC5883_MODE_SINGLE);
        pif_Delay1ms(50);
        if (pifHmc5883_ReadMag(p_owner, adc)) {                // Get the raw values in case the scales have already been changed.
			// Since the measurements are noisy, they should be averaged.
			xyz_total[AXIS_X] -= adc[AXIS_X];
			xyz_total[AXIS_Y] -= adc[AXIS_Y];
			xyz_total[AXIS_Z] -= adc[AXIS_Z];

			// Detect saturation.
			if (-4096 >= MIN(adc[AXIS_X], MIN(adc[AXIS_Y], adc[AXIS_Z]))) {
				bret = FALSE;
				break;              // Breaks out of the for loop.  No sense in continuing if we saturated.
			}
			i++;
        }
    }

    if (bret) {                	// Something went wrong so get a best guess
        if (xyz_total[AXIS_X]) p_owner->scale[AXIS_X] = fabsf(660.0f * HMC58X3_X_SELF_TEST_GAUSS * 2.0f * 10.0f / xyz_total[AXIS_X]);
        if (xyz_total[AXIS_Y]) p_owner->scale[AXIS_Y] = fabsf(660.0f * HMC58X3_Y_SELF_TEST_GAUSS * 2.0f * 10.0f / xyz_total[AXIS_Y]);
        if (xyz_total[AXIS_Z]) p_owner->scale[AXIS_Z] = fabsf(660.0f * HMC58X3_Z_SELF_TEST_GAUSS * 2.0f * 10.0f / xyz_total[AXIS_Z]);
    }

#ifndef PIF_NO_LOG
    pifLog_Printf(LT_INFO, "Mag scale: X=%f Y=%f Z=%f", (double)p_owner->scale[AXIS_X], (double)p_owner->scale[AXIS_Y], (double)p_owner->scale[AXIS_Z]);
#endif

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
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}

BOOL pifHmc5883_SetGain(PifHmc5883* p_owner, PifHmc5883Gain gain)
{
    if (!pifI2cDevice_WriteRegBit8(p_owner->_p_i2c, HMC5883_REG_CONFIG_B, HMC5883_CONFIG_B_GAIN, gain)) return FALSE;
	_changeGain(p_owner->__p_imu_sensor, gain);
    return TRUE;
}

BOOL pifHmc5883_ReadMag(PifHmc5883* p_owner, int16_t* p_mag)
{
	uint8_t data[6];

	if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, HMC5883_REG_STATUS, data)) return FALSE;
	if (!(data[0] & 1)) return FALSE;

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, HMC5883_REG_OUT_X_M, data, 6)) return FALSE;

	p_mag[AXIS_X] = (int16_t)((data[0] << 8) + data[1]) * p_owner->scale[AXIS_X];
	p_mag[AXIS_Z] = (int16_t)((data[2] << 8) + data[3]) * p_owner->scale[AXIS_Z];
	p_mag[AXIS_Y] = (int16_t)((data[4] << 8) + data[5]) * p_owner->scale[AXIS_Y];
	return TRUE;
}
