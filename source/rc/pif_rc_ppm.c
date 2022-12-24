#include "core/pif_list.h"
#include "rc/pif_rc_ppm.h"


static uint16_t _processRcPpm(PifRcPpm* p_owner, uint16_t diff)
{
	uint16_t rtn = 0;

	switch (p_owner->__process_step) {
	case 0:
		if (diff >= p_owner->__threshold_1us) p_owner->__process_step++;
		break;

	case 1:
		if (diff < p_owner->__threshold_1us) {
			p_owner->_channel++;
			if (p_owner->_channel < p_owner->__max_channel) {
				if (p_owner->_channel + 1 > p_owner->parent._channel_count) p_owner->parent._channel_count = p_owner->_channel + 1;
			}
		}
		else {
			p_owner->_channel = -1;
			p_owner->__process_step++;
		}
		break;

	case 2:
		if (diff < p_owner->__threshold_1us) {
			p_owner->_channel++;
			if (p_owner->_channel < p_owner->__max_channel) {
				if (p_owner->__valid_range.check) {
					if (diff >= p_owner->__valid_range.min && diff <= p_owner->__valid_range.max) {
						p_owner->__p_channel[p_owner->_channel] = diff;
						rtn = diff;
					}
				}
				else {
					p_owner->__p_channel[p_owner->_channel] = diff;
					rtn = diff;
				}

				if (rtn && p_owner->_channel + 1 >= p_owner->parent._channel_count) {
					if (p_owner->parent.__evt_receive) {
						(*p_owner->parent.__evt_receive)(&p_owner->parent, p_owner->__p_channel, p_owner->parent.__p_issuer);
					}
				}
			}
		}
		else {
			p_owner->_channel = -1;
		}
		break;
	}
	return rtn;
}


BOOL pifRcPpm_Init(PifRcPpm* p_owner, PifId id, uint8_t channel_count, uint16_t threshold_1us)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifRcPpm));

    p_owner->__p_channel = calloc(sizeof(uint16_t), channel_count);
    if (!p_owner->__p_channel) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._failsafe = FALSE;
	p_owner->_channel = -1;
	p_owner->__max_channel = channel_count;
    p_owner->__threshold_1us = threshold_1us;
    return TRUE;
}

void pifRcPpm_Clear(PifRcPpm* p_owner)
{
	if (p_owner->__p_channel) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
	}
}

BOOL pifRcPpm_SetValidRange(PifRcPpm* p_owner, uint32_t min, uint32_t max)
{
	p_owner->__valid_range.check = TRUE;
	p_owner->__valid_range.min = min;
	p_owner->__valid_range.max = max;
	return TRUE;
}

void pifRcPpm_ResetMeasureValue(PifRcPpm* p_owner)
{
	memset(p_owner->__pulse, 0, sizeof(p_owner->__pulse));
	p_owner->__ptr = 0;
	p_owner->__last_ptr = 0;
	p_owner->__count = 0;
}

uint16_t pifRcPpm_sigTick(PifRcPpm* p_owner, uint32_t time_us)
{
	uint16_t rtn = 0;

	p_owner->__pulse[p_owner->__ptr].falling = time_us;
	rtn = _processRcPpm(p_owner, p_owner->__pulse[p_owner->__ptr].falling - p_owner->__pulse[p_owner->__last_ptr].falling);
	p_owner->__last_ptr = p_owner->__ptr;
	p_owner->__ptr = (p_owner->__ptr + 1) & PIF_RC_PPM_DATA_MASK;

	if (p_owner->__count < PIF_RC_PPM_DATA_SIZE) p_owner->__count++;

	return rtn;
}
