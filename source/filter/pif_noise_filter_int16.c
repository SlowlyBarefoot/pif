#include "filter/pif_noise_filter_int16.h"


static void _clear(PifNoiseFilterInt16* p_owner)
{
	switch (p_owner->parent._type) {
	case NFT_WEIGHT_FACTOR:
		if (p_owner->__wf.value) {
			free(p_owner->__wf.value);
			p_owner->__wf.value = NULL;
		}
		break;

	case NFT_NOISE_CANCEL:
		if (p_owner->__nc.diff) {
			free(p_owner->__nc.diff);
			p_owner->__nc.diff = NULL;
		}
		break;

	default:
		break;
	}
}

static PifNoiseFilterValueP _processAverage(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNoiseFilterInt16* p_owner = (PifNoiseFilterInt16*)p_parent;

	p_owner->__current = (p_owner->__current + 1) % p_owner->_size;
	p_owner->__avg.sum -= p_owner->__buffer[p_owner->__current];
	p_owner->__buffer[p_owner->__current] = *(int16_t*)p_value;

	p_owner->__avg.sum += p_owner->__buffer[p_owner->__current];
	p_owner->_result = p_owner->__avg.sum / p_owner->_size;
	return &p_owner->_result;
}

static PifNoiseFilterValueP _processWeightFactor(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNoiseFilterInt16* p_owner = (PifNoiseFilterInt16*)p_parent;
	int i, n;
	int32_t sum;

	p_owner->__current = (p_owner->__current + 1) % p_owner->_size;
	p_owner->__buffer[p_owner->__current] = *(int16_t*)p_value;

	sum = 0;
	n = p_owner->__current;
	for (i = 0; i < p_owner->_size; i++) {
		sum += p_owner->__buffer[n] * p_owner->__wf.value[i];
		n = (n + 1) % p_owner->_size;
	}
	p_owner->_result = sum / p_owner->__wf.total;
	return &p_owner->_result;
}

static PifNoiseFilterValueP _processNoiseCancel(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNoiseFilterInt16* p_owner = (PifNoiseFilterInt16*)p_parent;
	int i, count;
	int32_t sum;
	int16_t* p_current;
	int16_t* p_before;
	int16_t current[3];

	p_before = p_owner->__nc.diff + p_owner->__current * 3;

	current[0] = *(int16_t*)p_value - p_owner->__buffer[p_owner->__current];

	current[1] = current[0] - p_before[0];
	if (current[1] < 0) current[1] = -current[1];

	current[2] = current[0] + p_before[0];
	if (current[2] < 0) current[2] = -current[2];

	if (current[1] > current[2]) {
		p_owner->__buffer[p_owner->__current] = *(int16_t*)p_value;

		p_current = p_before;
		p_before = p_owner->__nc.diff + p_owner->__nc.before * 3;

		p_current[0] = p_owner->__buffer[p_owner->__current] - p_owner->__buffer[p_owner->__nc.before];

		p_current[1] = p_current[0] - p_before[0];
		if (p_current[1] < 0) p_current[1] = -p_current[1];

		p_current[2] = p_current[0] + p_before[0];
		if (p_current[2] < 0) p_current[2] = -p_current[2];
	}
	else {
		p_owner->__nc.before = p_owner->__current;
		p_owner->__current = (p_owner->__current + 1) % p_owner->_size;
		p_owner->__buffer[p_owner->__current] = *(int16_t*)p_value;

		p_current = p_owner->__nc.diff + p_owner->__current * 3;

		p_current[0] = current[0];
		p_current[1] = current[1];
		p_current[2] = current[2];
	}

	sum = 0;
	count = 0;
	for (i = 0; i < p_owner->_size; i++) {
		sum += p_owner->__buffer[i];
		count++;
	}
	if (count > 0) {
		p_owner->_result = sum / count;
	}
	return &p_owner->_result;
}

BOOL pifNoiseFilterInt16_Init(PifNoiseFilterInt16* p_owner, uint8_t size)
{
	if (!p_owner || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifNoiseFilterInt16));

	p_owner->__buffer = calloc(size, sizeof(int16_t));
	if (!p_owner->__buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	pifNoiseFilter_Init(&p_owner->parent, NFT_AVERAGE);
    p_owner->_size = size;
	p_owner->__current = 0;

	p_owner->parent.__fn_process = _processAverage;
    return TRUE;
}

void pifNoiseFilterInt16_Clear(PifNoiseFilterInt16* p_owner)
{
	_clear(p_owner);
	if (p_owner->__buffer) {
		free(p_owner->__buffer);
		p_owner->__buffer = NULL;
	}
}

BOOL pifNoiseFilterInt16_SetWeightFactor(PifNoiseFilterInt16* p_owner, ...)
{
	int i, n;
	va_list ap;

	_clear(p_owner);

	if (!(p_owner->_size & 1)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__wf.value = calloc(p_owner->_size, sizeof(int8_t));
	if (!p_owner->__wf.value) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	va_start(ap, p_owner);
	p_owner->__wf.total = 0;
	n = p_owner->_size / 2;
	for (i = 0; i < p_owner->_size; i++) {
		p_owner->__wf.value[n] = va_arg(ap, int);
		p_owner->__wf.total += p_owner->__wf.value[n];
		n = (n + 1) % p_owner->_size;
	}
	va_end(ap);

	p_owner->parent._type = NFT_WEIGHT_FACTOR;
	p_owner->parent.__fn_process = _processWeightFactor;
	return TRUE;
}

BOOL pifNoiseFilterInt16_SetNoiseCancel(PifNoiseFilterInt16* p_owner)
{
	_clear(p_owner);

	if (p_owner->_size < 3 || p_owner->_size > 32) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__nc.diff = calloc(p_owner->_size * 3, sizeof(int16_t));
	if (!p_owner->__nc.diff) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_owner->__nc.before = 0;

	p_owner->parent._type = NFT_NOISE_CANCEL;
	p_owner->parent.__fn_process = _processNoiseCancel;
	return TRUE;
}
