// SPDX-License-Identifier: BSD-3-Clause AND MIT

#include "sensor/pif_dps310.h"

#include <math.h>


static const int32_t _scaling_facts[] = { 524288, 1572864, 3670016, 7864320, 253952, 516096, 1040384, 2088960 };


/**
 * @fn _compensate_T
 * @brief Internal helper that supports compensate t logic.
 * @param p_owner Pointer to the owner instance.
 * @param adc_T Raw ADC temperature sample.
 * @return Computed floating-point value.
 */
static float _compensate_T(PifDps310* p_owner, int32_t adc_T)
{
	float temp = adc_T;

	//scale temperature according to scaling table and oversampling
	temp /= _scaling_facts[p_owner->_osrs_t];

	//update last measured temperature
	//it will be used for pressure compensation
	p_owner->__last_temp = temp;

	//Calculate compensated temperature
	temp = p_owner->__c0_half + p_owner->__c1 * temp;

	return temp;
}

// Returns pressure in hPa as floating point.
/**
 * @fn _compensate_P
 * @brief Internal helper that supports compensate p logic.
 * @param p_owner Pointer to the owner instance.
 * @param adc_P Raw ADC pressure sample.
 * @return Computed floating-point value.
 */
static float _compensate_P(PifDps310* p_owner, int32_t adc_P)
{
	float prs = adc_P;

	//scale pressure according to scaling table and oversampling
	prs /= _scaling_facts[p_owner->_osrs_p];

	//Calculate compensated pressure
	prs = p_owner->__c00 + prs * (p_owner->__c10 + prs * (p_owner->__c20 + prs * p_owner->__c30)) 
			+ p_owner->__last_temp * (p_owner->__c01 + prs * (p_owner->__c11 + prs * p_owner->__c21));

	// The compensation comes out in Pa.
	return prs / 100.0f;
}

/**
 * @fn _getTwosComplement
 * @brief Retrieves the current value from get twos complement.
 * @param raw Raw register value read from the device.
 * @param length Number of valid bits in the raw value.
 * @return Computed integer value.
 */
// Sign-extends a two's complement value of the given width (at most 31 bits) to 32 bits.
static int32_t _getTwosComplement(uint32_t raw, uint8_t length)
{
	uint32_t sign = (uint32_t)1 << (length - 1);

	raw &= (sign << 1) - 1;
	return (int32_t)(raw ^ sign) - (int32_t)sign;
}

/**
 * @fn _doTask
 * @brief Internal helper that supports do task logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifDps310* p_owner = p_task->_p_client;
	uint8_t data[6];
	uint32_t delay = 1000UL;
	uint16_t gap;
	float pressure;
	float temperature;

	switch (p_owner->__state) {
	case DPS310_STATE_READY:
		if (pifDeviceReg_ReadRegByte(p_owner->_p_device, DPS310_REG_MEAS_CFG, data)) {
			if (((data[0]) & DPS310_PRS_RDY_MASK) && (data[0] & DPS310_TMP_RDY_MASK)) {
				p_owner->__start_time = pif_cumulative_timer1ms;
				p_owner->__state = DPS310_STATE_READ;
				delay = 1;
			}
		}
		break;

	case DPS310_STATE_READ:
		if (pifDeviceReg_ReadRegBytes(p_owner->_p_device, DPS310_REG_PRS_B2, data, 6)) {
			p_owner->__raw_pressure = _getTwosComplement((((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2]), 24);
			p_owner->__raw_temperature = _getTwosComplement((((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5]), 24);
			p_owner->__state = DPS310_STATE_CALCURATE;
			delay = 1;
		}
		break;

	case DPS310_STATE_CALCURATE:
		temperature = _compensate_T(p_owner, p_owner->__raw_temperature);
		pressure = _compensate_P(p_owner, p_owner->__raw_pressure);

		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);

		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) {
			delay = (p_owner->__read_period - gap) * 1000UL;
		}
		else {
			delay = 1;
		}
		p_owner->__state = DPS310_STATE_READY;
		break;

	default:
		break;
	}
	return delay;
}

BOOL pifDps310_Config(PifDps310* p_owner, PifId id)
{
	uint8_t data;
	uint8_t buffer[18];

	if (!p_owner || !p_owner->_p_device) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, DPS310_REG_PRODUCT_ID, &data)) return FALSE;
	if (data != DPS310_PRODUCT_ID_CONST) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	// Held with the CPU on purpose: this runs once at start-up, and the coefficients cannot be
	// read until the device says they are ready, about 40ms after the reset.
	if (!pifDeviceReg_WriteRegBit8(p_owner->_p_device, DPS310_REG_RESET, DPS310_SOFT_RST_MASK, DPS310_RESET_SOFT_RST)) return FALSE;
	pif_Delay1ms(40);

	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, DPS310_REG_MEAS_CFG, &data)) return FALSE;
	if (!(data & DPS310_COEF_RDY_MASK) || !(data & DPS310_SENSOR_RDY_MASK)) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, DPS310_REG_COEF, buffer, 18)) return FALSE;

	//compose coefficients from buffer content
	// Kept as a float: an odd c0 would lose 0.5 DegC to an integer halving.
	p_owner->__c0_half = _getTwosComplement(((uint32_t)buffer[0] << 4) | (((uint32_t)buffer[1] >> 4) & 0x0F), 12) / 2.0f;

	//now do the same thing for all other coefficients
	p_owner->__c1 = _getTwosComplement((((uint32_t)buffer[1] & 0x0F) << 8) | (uint32_t)buffer[2], 12);
	p_owner->__c00 = _getTwosComplement(((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) | (((uint32_t)buffer[5] >> 4) & 0x0F), 20);
	p_owner->__c10 = _getTwosComplement((((uint32_t)buffer[5] & 0x0F) << 16) | ((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7], 20);

	p_owner->__c01 = _getTwosComplement(((uint32_t)buffer[8] << 8) | (uint32_t)buffer[9], 16);

	p_owner->__c11 = _getTwosComplement(((uint32_t)buffer[10] << 8) | (uint32_t)buffer[11], 16);
	p_owner->__c20 = _getTwosComplement(((uint32_t)buffer[12] << 8) | (uint32_t)buffer[13], 16);
	p_owner->__c21 = _getTwosComplement(((uint32_t)buffer[14] << 8) | (uint32_t)buffer[15], 16);
	p_owner->__c30 = _getTwosComplement(((uint32_t)buffer[16] << 8) | (uint32_t)buffer[17], 16);

	// Some parts read a wrong temperature after a reset, an IC bug that Infineon's own driver works
	// around with this sequence. It has to be written while the device is idle, which it is after
	// the reset; the background measurement started below then takes a fresh temperature.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, 0x0E, 0xA5)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, 0x0F, 0x96)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, 0x62, 0x02)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, 0x0E, 0x00)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, 0x0F, 0x00)) return FALSE;

	// In background mode the measurements of a second have to fit in it: 16 times takes 27.6ms,
	// so 8Hz for both is 8 * 27.6 * 2 = 442ms, where 32Hz would be 1.77s.
	if (!pifDps310_SetPressureCfg(p_owner, DPS310_PM_PRC_16, DPS310_PM_RATE_8HZ)) return FALSE;
	if (!pifDps310_SetTemperatureCfg(p_owner, DPS310_TMP_PRC_16, DPS310_TMP_RATE_8HZ)) return FALSE;

	if (!pifDps310_SensorOperatingMode(p_owner, DPS310_MEAS_CTRL_CONT_PRS_TMP)) return FALSE;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

BOOL pifDps310_SetPressureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate)
{
	if (osrs > DPS310_PM_PRC_128 || (rate & ~DPS310_PM_RATE_MASK)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, DPS310_REG_PRS_CFG, osrs | rate)) return FALSE;
	// The result is shifted when, and only when, it is oversampled more than 8 times.
	if (!pifDeviceReg_WriteRegBit8(p_owner->_p_device, DPS310_REG_CFG_REG, DPS310_P_SHIFT_MASK,
			DPS310_P_SHIFT(osrs > DPS310_PM_PRC_8 ? 1 : 0))) return FALSE;
	p_owner->_osrs_p = osrs;
	return TRUE;
}

BOOL pifDps310_SetTemperatureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate)
{
	uint8_t data;

	if (osrs > DPS310_TMP_PRC_128 || (rate & ~DPS310_TMP_RATE_MASK)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!pifDeviceReg_ReadRegBit8(p_owner->_p_device, DPS310_REG_COEF_SPCE, DPS310_TMP_COEF_SPCE_MASK, &data)) return FALSE;
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, DPS310_REG_TMP_CFG, osrs | rate | data)) return FALSE;
	if (!pifDeviceReg_WriteRegBit8(p_owner->_p_device, DPS310_REG_CFG_REG, DPS310_T_SHIFT_MASK,
			DPS310_T_SHIFT(osrs > DPS310_TMP_PRC_8 ? 1 : 0))) return FALSE;
	p_owner->_osrs_t = osrs;
	return TRUE;
}

BOOL pifDps310_SensorOperatingMode(PifDps310* p_owner, uint8_t meas_ctrl)
{
	return pifDeviceReg_WriteRegBit8(p_owner->_p_device, DPS310_REG_MEAS_CFG, DPS310_MEAS_CTRL_MASK, meas_ctrl);
}

BOOL pifDps310_ReadRawData(PifDps310* p_owner, int32_t* p_pressure, int32_t* p_temperature)
{
	uint8_t data[6];

	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, DPS310_REG_MEAS_CFG, data)) return FALSE;
	if (!(data[0] & DPS310_PRS_RDY_MASK) || !(data[0] & DPS310_TMP_RDY_MASK)) return FALSE;

	if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, DPS310_REG_PRS_B2, data, 6)) return FALSE;
	*p_pressure = _getTwosComplement((((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2]), 24);
	*p_temperature = _getTwosComplement((((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5]), 24);
	return TRUE;
}

BOOL pifDps310_ReadBarometric(PifDps310* p_owner, float* p_pressure, float* p_temperature)
{
	int32_t pressure, temperature;
	float value;

	if (!pifDps310_ReadRawData(p_owner, &pressure, &temperature)) return FALSE;

	// The temperature is compensated even when it is not asked for: that is what updates the
	// __last_temp the pressure compensation depends on.
	value = _compensate_T(p_owner, temperature);
    if (p_temperature) *p_temperature = value;
    if (p_pressure) *p_pressure = _compensate_P(p_owner, pressure);
	return TRUE;
}

BOOL pifDps310_AttachTaskForReading(PifDps310* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(id, TM_PERIOD, read_period * 1000UL, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "DPS310";

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->__state = DPS310_STATE_READY;
    return TRUE;
}

