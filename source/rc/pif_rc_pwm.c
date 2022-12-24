#include "core/pif_list.h"
#include "rc/pif_rc_pwm.h"


static uint16_t _processRcPwm(PifRcPwm* p_owner, uint8_t channel, uint32_t time_us)
{
	uint16_t rtn = 0;
	uint16_t value;
	PifRcPwmData* p_data;

	switch (p_owner->__process_step) {
	case 0:
		if (channel == 0) {
			p_owner->parent._channel_count = 1;
			p_owner->__process_step++;
		}
		break;

	case 1:
		if (channel + 1 > p_owner->parent._channel_count) p_owner->parent._channel_count = channel + 1;
		else p_owner->__process_step++;
		break;
	
	case 2:
		p_data = &p_owner->__p_data[channel];
		p_data->pulse[p_data->ptr].falling = time_us;
		p_data->last_ptr = p_data->ptr;
		p_data->ptr = (p_data->ptr + 1) & PIF_RC_PWM_DATA_MASK;

		value = p_data->pulse[p_data->last_ptr].falling - p_data->pulse[p_data->last_ptr].rising;
		if (p_owner->__valid_range.check) {
			if (value >= p_owner->__valid_range.min && value <= p_owner->__valid_range.max) {
				p_owner->__p_channel[channel] = value;
				rtn = value;
			}
			else {
				p_owner->__process_step++;
			}
		}
		else {
			p_owner->__p_channel[channel] = value;
			rtn = value;
		}

		if (channel + 1 == p_owner->parent._channel_count) {
			if (rtn && p_owner->parent.__evt_receive) {
				(*p_owner->parent.__evt_receive)(&p_owner->parent, p_owner->__p_channel, p_owner->parent.__p_issuer);
			}
		}
		break;

	case 3:
		if (channel + 1 == p_owner->parent._channel_count) {
			pifRcPwm_ResetMeasureValue(p_owner);
			p_owner->__process_step = 2;
		}
		break;
	}
	return rtn;
}


BOOL pifRcPwm_Init(PifRcPwm* p_owner, PifId id, uint8_t channel_count)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifRcPwm));

	p_owner->__p_data = calloc(sizeof(PifRcPwmData), channel_count);
    if (!p_owner->__p_data) {
		pif_error = E_OUT_OF_HEAP;
        goto fail;
	}

    p_owner->__p_channel = calloc(sizeof(uint16_t), channel_count);
    if (!p_owner->__p_channel) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._failsafe = FALSE;
	p_owner->__max_channel = channel_count;
    return TRUE;

fail:
	pifRcPwm_Clear(p_owner);
	return FALSE;
}

void pifRcPwm_Clear(PifRcPwm* p_owner)
{
	if (p_owner->__p_channel) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
	}
	if (p_owner->__p_data) {
		free(p_owner->__p_data);
		p_owner->__p_data = NULL;
	}
}

BOOL pifRcPwm_SetValidRange(PifRcPwm* p_owner, uint32_t min, uint32_t max)
{
	p_owner->__valid_range.check = TRUE;
	p_owner->__valid_range.min = min;
	p_owner->__valid_range.max = max;
	return TRUE;
}

void pifRcPwm_ResetMeasureValue(PifRcPwm* p_owner)
{
	int i;
	PifRcPwmData* p_data;

	for (i = 0; i < p_owner->__max_channel; i++) {
		p_data = &p_owner->__p_data[i];
		memset(p_data, 0, sizeof(PifRcPwmData));
		p_data->ptr = 0;
		p_data->last_ptr = 0;
	}
}

uint16_t pifRcPwm_sigEdge(PifRcPwm* p_owner, uint8_t channel, PifPulseState state, uint32_t time_us)
{
	uint16_t rtn = 0;

	if (channel >= p_owner->__max_channel) return FALSE;

	if (state == PS_RISING_EDGE) {
		p_owner->__p_data[channel].pulse[p_owner->__p_data[channel].ptr].rising = time_us;
	}
	else {
		rtn = _processRcPwm(p_owner, channel, time_us);
	}

	return rtn;
}
