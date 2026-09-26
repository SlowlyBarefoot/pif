#include "sensor/pif_adc.h"


#define NO_VREFINT			0xFF
#define MAX_FILTER_SHIFT	8


/**
 * @fn _fullScale
 * @param p_owner Pointer to the instance.
 * @return Highest conversion, with _filter_shift bits of fraction, as the filtered values carry.
 */
static uint32_t _fullScale(PifAdc* p_owner)
{
	return ((1UL << p_owner->_resolution) - 1) << p_owner->_filter_shift;
}

/**
 * @fn _filter
 * @brief Moves the filtered value of a channel towards a new reading.
 * @param p_owner Pointer to the instance.
 * @param p_channel Channel.
 * @param raw New reading.
 */
static void _filter(PifAdc* p_owner, PifAdcChannel* p_channel, uint16_t raw)
{
	uint32_t target = (uint32_t)raw << p_owner->_filter_shift;

	// Started at the first reading rather than at 0, which would take a few time constants to
	// climb out of and read as a flat battery meanwhile.
	if (!p_channel->__primed) {
		p_channel->__filtered = target;
		p_channel->__primed = TRUE;
	}
	else if (target >= p_channel->__filtered) {
		p_channel->__filtered += (target - p_channel->__filtered) >> p_owner->_filter_shift;
	}
	else {
		p_channel->__filtered -= (p_channel->__filtered - target) >> p_owner->_filter_shift;
	}
	p_channel->_raw = p_channel->__filtered >> p_owner->_filter_shift;
}

/**
 * @fn _convert
 * @brief Converts the filtered value of a channel into millivolts and into its value.
 * @param p_owner Pointer to the instance.
 * @param p_channel Channel.
 */
static void _convert(PifAdc* p_owner, PifAdcChannel* p_channel)
{
	uint64_t scaled;
	int64_t delta;

	p_channel->_millivolt = (uint16_t)(((uint64_t)p_channel->__filtered * p_owner->_vref_mv + _fullScale(p_owner) / 2) / _fullScale(p_owner));

	switch (p_channel->_type) {
	case ACH_VOLTAGE:
		p_channel->_value = (int32_t)((int64_t)p_channel->_millivolt * p_channel->__mul / p_channel->__div) + p_channel->__offset;
		break;

	case ACH_VREFINT:
		p_channel->_value = p_owner->_vref_mv;
		break;

	case ACH_TEMPERATURE:
		// The reading as it would have been at the supply of the calibration, since the sensor
		// gives a voltage and the ADC measures it against whatever the supply is now.
		scaled = (uint64_t)p_channel->__filtered * p_owner->_vref_mv / p_channel->__cal_mv;
		delta = (int64_t)scaled - ((int64_t)p_channel->__cal1_raw << p_owner->_filter_shift);
		p_channel->_value = p_channel->__cal1_t + (int32_t)(delta * (p_channel->__cal2_t - p_channel->__cal1_t) /
				(((int64_t)p_channel->__cal2_raw - p_channel->__cal1_raw) << p_owner->_filter_shift));
		break;
	}
}

/**
 * @fn _doTask
 * @brief Samples every channel.
 * @param p_task Task whose client is the instance.
 * @return 0, as the period is fixed.
 */
static uint32_t _doTask(PifTask* p_task)
{
	pifAdc_Sample((PifAdc*)p_task->_p_client);
	return 0;
}

/**
 * @fn _checkChannel
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @return TRUE if the instance and the channel exist, otherwise FALSE with E_INVALID_PARAM.
 */
static BOOL _checkChannel(PifAdc* p_owner, uint8_t index)
{
	if (!p_owner || index >= p_owner->_channel_count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _measureVref
 * @brief Works out the supply from the filtered reading of the internal reference.
 * @param p_owner Pointer to the instance.
 */
static void _measureVref(PifAdc* p_owner)
{
	PifAdcChannel* p_vrefint = &p_owner->_channel[p_owner->__vrefint_index];

	// A reading of 0 is a reference that is not being converted, not an infinite supply, so the
	// last supply is kept.
	if (!p_vrefint->__filtered) return;
	p_owner->_vref_mv = (uint16_t)((((uint64_t)p_vrefint->__cal_mv * p_vrefint->__cal1_raw) << p_owner->_filter_shift) /
			p_vrefint->__filtered);
}

BOOL pifAdc_Init(PifAdc* p_owner, PifId id, uint8_t channel_count, uint8_t resolution, uint16_t vref_mv, PifActAdcRead act_read)
{
	uint8_t i;

	if (!p_owner || !channel_count || channel_count > PIF_ADC_MAX_CHANNELS || resolution < 8 || resolution > 16 ||
			!vref_mv || !act_read) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifAdc));

	for (i = 0; i < channel_count; i++) {
		p_owner->_channel[i]._type = ACH_VOLTAGE;
		p_owner->_channel[i].__mul = 1;
		p_owner->_channel[i].__div = 1;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_channel_count = channel_count;
	p_owner->_resolution = resolution;
	p_owner->_vref_mv = vref_mv;
	p_owner->__vrefint_index = NO_VREFINT;
	p_owner->__act_read = act_read;
	return TRUE;
}

void pifAdc_Clear(PifAdc* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

void pifAdc_AttachActStart(PifAdc* p_owner, PifActAdcStart act_start)
{
	p_owner->__act_start = act_start;
	if (act_start) (*act_start)(p_owner);
}

BOOL pifAdc_AttachTask(PifAdc* p_owner, uint32_t period1us, BOOL start)
{
	if (!p_owner || !period1us) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_p_task) {
		pif_error = E_ALREADY_ATTACHED;
		return FALSE;
	}

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1us, _doTask, p_owner, start);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "ADC";
	return TRUE;
}

BOOL pifAdc_SetFilter(PifAdc* p_owner, uint8_t shift)
{
	uint8_t i;

	if (!p_owner || shift > MAX_FILTER_SHIFT) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// The filtered values keep their fraction in as many bits as the shift, so they are carried
	// over to the new number of bits rather than started again.
	for (i = 0; i < p_owner->_channel_count; i++) {
		p_owner->_channel[i].__filtered = ((uint32_t)p_owner->_channel[i]._raw) << shift;
	}
	p_owner->_filter_shift = shift;
	return TRUE;
}

BOOL pifAdc_SetScale(PifAdc* p_owner, uint8_t index, int32_t mul, int32_t div, int32_t offset)
{
	PifAdcChannel* p_channel;

	if (!_checkChannel(p_owner, index)) return FALSE;
	if (!div) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_channel = &p_owner->_channel[index];
	if (p_owner->__vrefint_index == index) p_owner->__vrefint_index = NO_VREFINT;
	p_channel->_type = ACH_VOLTAGE;
	p_channel->__mul = mul;
	p_channel->__div = div;
	p_channel->__offset = offset;
	return TRUE;
}

BOOL pifAdc_SetVrefint(PifAdc* p_owner, uint8_t index, uint16_t cal_raw, uint16_t cal_mv)
{
	PifAdcChannel* p_channel;

	if (!_checkChannel(p_owner, index)) return FALSE;
	if (!cal_raw || !cal_mv) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_channel = &p_owner->_channel[index];
	p_channel->_type = ACH_VREFINT;
	p_channel->__cal1_raw = cal_raw;
	p_channel->__cal_mv = cal_mv;
	p_owner->__vrefint_index = index;
	return TRUE;
}

void pifAdc_SetReference(PifAdc* p_owner, uint16_t vref_mv)
{
	if (vref_mv) p_owner->_vref_mv = vref_mv;
}

BOOL pifAdc_SetTemperature(PifAdc* p_owner, uint8_t index, uint16_t cal1_raw, int16_t cal1_t, uint16_t cal2_raw, int16_t cal2_t, uint16_t cal_mv)
{
	PifAdcChannel* p_channel;

	if (!_checkChannel(p_owner, index)) return FALSE;
	if (cal1_raw == cal2_raw || !cal_mv) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_channel = &p_owner->_channel[index];
	if (p_owner->__vrefint_index == index) p_owner->__vrefint_index = NO_VREFINT;
	p_channel->_type = ACH_TEMPERATURE;
	p_channel->__cal1_raw = cal1_raw;
	p_channel->__cal1_t = cal1_t;
	p_channel->__cal2_raw = cal2_raw;
	p_channel->__cal2_t = cal2_t;
	p_channel->__cal_mv = cal_mv;
	return TRUE;
}

void pifAdc_Sample(PifAdc* p_owner)
{
	uint8_t i;

	for (i = 0; i < p_owner->_channel_count; i++) {
		_filter(p_owner, &p_owner->_channel[i], (*p_owner->__act_read)(p_owner, i));
	}

	// The supply first, since every other channel is converted against it.
	if (p_owner->__vrefint_index != NO_VREFINT) _measureVref(p_owner);

	for (i = 0; i < p_owner->_channel_count; i++) {
		_convert(p_owner, &p_owner->_channel[i]);
	}

	if (p_owner->__act_start) (*p_owner->__act_start)(p_owner);
	if (p_owner->evt_sample) (*p_owner->evt_sample)(p_owner);
}

int32_t pifAdc_SampleChannel(PifAdc* p_owner, uint8_t index)
{
	PifAdcChannel* p_channel;

	if (index >= p_owner->_channel_count) return 0;
	p_channel = &p_owner->_channel[index];

	_filter(p_owner, p_channel, (*p_owner->__act_read)(p_owner, index));
	if (index == p_owner->__vrefint_index) _measureVref(p_owner);
	_convert(p_owner, p_channel);
	return p_channel->_value;
}

uint16_t pifAdc_GetRaw(PifAdc* p_owner, uint8_t index)
{
	return index < p_owner->_channel_count ? p_owner->_channel[index]._raw : 0;
}

uint16_t pifAdc_GetMilliVolt(PifAdc* p_owner, uint8_t index)
{
	return index < p_owner->_channel_count ? p_owner->_channel[index]._millivolt : 0;
}

int32_t pifAdc_GetValue(PifAdc* p_owner, uint8_t index)
{
	return index < p_owner->_channel_count ? p_owner->_channel[index]._value : 0;
}
