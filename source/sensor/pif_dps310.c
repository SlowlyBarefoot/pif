#include "core/pif_task.h"
#include "sensor/pif_dps310.h"

#include <math.h>


const int32_t _scaling_facts[] = { 524288, 1572864, 3670016, 7864320, 253952, 516096, 1040384, 2088960 };


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
static float _compensate_P(PifDps310* p_owner, int32_t adc_P)
{
	float prs = adc_P;

	//scale pressure according to scaling table and oversampling
	prs /= _scaling_facts[p_owner->_osrs_p];

	//Calculate compensated pressure
	prs = p_owner->__c00 + prs * (p_owner->__c10 + prs * (p_owner->__c20 + prs * p_owner->__c30)) 
			+ p_owner->__last_temp * (p_owner->__c01 + prs * (p_owner->__c11 + prs * p_owner->__c21));

	return prs;
}

static int32_t _getTwosComplement(uint32_t raw, uint8_t length)
{
	if (raw & ((int)1 << (length - 1)))	{
		return ((int32_t)raw) - ((int32_t)1 << length);
	}
	return raw;
}

static uint32_t _doTask(PifTask* p_task)
{
	PifDps310* p_owner = p_task->_p_client;
	uint8_t data[6];
	uint16_t delay = 1000;
	uint16_t gap;
	float pressure;
	float temperature;

	switch (p_owner->__state) {
	case DPS310_STATE_READY:
		if ((p_owner->_fn.read_byte)(p_owner->_fn.p_device, DPS310_REG_MEAS_CFG, data)) {
			if (((data[0]) & DPS310_PRS_RDY_MASK) && (data[0] & DPS310_TMP_RDY_MASK)) {
				p_owner->__start_time = pif_cumulative_timer1ms;
				p_owner->__state = DPS310_STATE_READ;
			}
		}
		break;

	case DPS310_STATE_READ:
		if ((p_owner->_fn.read_bytes)(p_owner->_fn.p_device, DPS310_REG_PRS_B2, data, 6)) {
			p_owner->__raw_pressure = _getTwosComplement((((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2]), 24);
			p_owner->__raw_temperature = _getTwosComplement((((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5]), 24);
			p_owner->__state = DPS310_STATE_CALCURATE;
			delay = 1;
		}
		break;

	case DPS310_STATE_CALCURATE:
		temperature = _compensate_T(p_owner, p_owner->__raw_temperature);
		pressure = _compensate_P(p_owner, p_owner->__raw_pressure);

		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature * 100);

		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) {
			delay = (p_owner->__read_period - gap) * 1000;
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

	if (!p_owner || !p_owner->_fn.p_device 
			|| !p_owner->_fn.read_byte || !p_owner->_fn.read_bytes || !p_owner->_fn.read_bit 
			|| !p_owner->_fn.write_byte || !p_owner->_fn.write_bytes || !p_owner->_fn.write_bit) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	if (!(p_owner->_fn.write_bit)(p_owner->_fn.p_device, DPS310_REG_RESET, DPS310_RESET_SOFT_RST, TRUE)) return FALSE;
	pif_Delay1ms(40);

	if (!(p_owner->_fn.read_byte)(p_owner->_fn.p_device, DPS310_REG_MEAS_CFG, &data)) return FALSE;
	if (!(data & DPS310_COEF_RDY_MASK) || !(data & DPS310_SENSOR_RDY_MASK)) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, DPS310_REG_COEF, buffer, 18)) return FALSE;

	//compose coefficients from buffer content
	p_owner->__c0_half = _getTwosComplement(((uint32_t)buffer[0] << 4) | (((uint32_t)buffer[1] >> 4) & 0x0F), 12) / 2;

	//now do the same thing for all other coefficients
	p_owner->__c1 = _getTwosComplement((((uint32_t)buffer[1] & 0x0F) << 8) | (uint32_t)buffer[2], 12);
	p_owner->__c00 = _getTwosComplement(((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) | (((uint32_t)buffer[5] >> 4) & 0x0F), 20);
	p_owner->__c10 = _getTwosComplement((((uint32_t)buffer[5] & 0x0F) << 16) | ((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7], 20);

	p_owner->__c01 = _getTwosComplement(((uint32_t)buffer[8] << 8) | (uint32_t)buffer[9], 16);

	p_owner->__c11 = _getTwosComplement(((uint32_t)buffer[10] << 8) | (uint32_t)buffer[11], 16);
	p_owner->__c20 = _getTwosComplement(((uint32_t)buffer[12] << 8) | (uint32_t)buffer[13], 16);
	p_owner->__c21 = _getTwosComplement(((uint32_t)buffer[14] << 8) | (uint32_t)buffer[15], 16);
	p_owner->__c30 = _getTwosComplement(((uint32_t)buffer[16] << 8) | (uint32_t)buffer[17], 16);

	if (!pifDps310_SetPressureCfg(p_owner, DPS310_PM_PRC_16, DPS310_PM_RATE_32HZ)) return FALSE;
	if (!pifDps310_SetTemperatureCfg(p_owner, DPS310_TMP_PRC_16, DPS310_TMP_RATE_32HZ)) return FALSE;

	if (!(p_owner->_fn.write_byte)(p_owner->_fn.p_device, DPS310_REG_CFG_REG, DPS310_P_SHIFT(1) | DPS310_T_SHIFT(1))) return FALSE;

	if (!pifDps310_SensorOperatingMode(p_owner, DPS310_MEAS_CTRL_CONT_PRS_TMP)) return FALSE;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

BOOL pifDps310_SetPressureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate)
{
	if (!(p_owner->_fn.write_byte)(p_owner->_fn.p_device, DPS310_REG_PRS_CFG, osrs | rate)) return FALSE;
	p_owner->_osrs_p = osrs;
	return TRUE;
}

BOOL pifDps310_SetTemperatureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate)
{
	uint8_t data;

	if (!(p_owner->_fn.read_bit)(p_owner->_fn.p_device, DPS310_REG_COEF_SPCE, DPS310_TMP_COEF_SPCE_MASK, &data)) return FALSE;
	if (!(p_owner->_fn.write_byte)(p_owner->_fn.p_device, DPS310_REG_TMP_CFG, osrs | rate | data)) return FALSE;
	p_owner->_osrs_t = osrs;
	return TRUE;
}

BOOL pifDps310_SensorOperatingMode(PifDps310* p_owner, uint8_t meas_ctrl)
{
	return (p_owner->_fn.write_bit)(p_owner->_fn.p_device, DPS310_REG_MEAS_CFG, DPS310_MEAS_CTRL_MASK, meas_ctrl);
}

BOOL pifDps310_ReadRawData(PifDps310* p_owner, int32_t* p_pressure, int32_t* p_temperature)
{
	uint8_t data[6];

	if (!(p_owner->_fn.read_byte)(p_owner->_fn.p_device, DPS310_REG_MEAS_CFG, data)) return FALSE;
	if (!(data[0] & DPS310_PRS_RDY_MASK) || !(data[0] & DPS310_TMP_RDY_MASK)) return FALSE;

	if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, DPS310_REG_PRS_B2, data, 6)) return FALSE;
	*p_pressure = _getTwosComplement((((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2]), 24);
	*p_temperature = _getTwosComplement((((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5]), 24);
	return TRUE;
}

BOOL pifDps310_ReadBarometric(PifDps310* p_owner, float* p_pressure, float* p_temperature)
{
	int32_t pressure, temperature;

	if (!pifDps310_ReadRawData(p_owner, &pressure, &temperature)) return FALSE;

    if (p_temperature) *p_temperature = _compensate_T(p_owner, temperature);
    if (p_pressure) *p_pressure = _compensate_P(p_owner, pressure);
	return TRUE;
}

BOOL pifDps310_AttachTaskForReading(PifDps310* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(id, TM_PERIOD, read_period * 1000, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "DPS310";

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->__state = DPS310_STATE_READY;
    return TRUE;
}

