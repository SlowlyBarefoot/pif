#include "filter/pif_noise_filter_int16.h"


static BOOL _checkParam(PifNoiseFilter* p_parent, BOOL check_size)
{
	if (!p_parent || !check_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_parent->_last >= p_parent->_count) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}
	return TRUE;
}

static BOOL _allocBuffer(PifNfInt16Common* p_common, uint8_t size)
{
	p_common->p_buffer = calloc(size, sizeof(int16_t));
	if (!p_common->p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_common->size = size;
	return TRUE;
}

static void _clearAverage(PifNoiseFilterMethod* p_method)
{
	PifNfInt16Common* p_common = &((PifNfInt16Average*)p_method)->common;

	if (p_common->p_buffer) {
		free(p_common->p_buffer);
		p_common->p_buffer = NULL;
	}
}

static void _resetAverage(PifNoiseFilterMethod* p_method)
{
	PifNfInt16Average* p_owner = (PifNfInt16Average*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;

	memset(p_common->p_buffer, 0, p_common->size * sizeof(int16_t));
	p_common->current = 0;
	p_owner->len = 0;
}

static PifNoiseFilterValueP _processAverage(PifNoiseFilterMethod* p_method, PifNoiseFilterValueP p_value)
{
	PifNfInt16Average* p_owner = (PifNfInt16Average*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;
	uint8_t min_p, max_p, i;
	int16_t min_v, max_v;
	int32_t sum;

	p_common->p_buffer[p_common->current] = *(int16_t*)p_value;
	p_common->current = (p_common->current + 1) % p_common->size;
	if (p_owner->len < p_common->size) {
		p_owner->len++;
		return NULL;
	}

	min_p = max_p = 0;
	min_v = max_v = p_common->p_buffer[0];
	for (i = 1; i < p_common->size; i++) {
		if (p_common->p_buffer[i] < min_v) {
			min_p = i;
			min_v = p_common->p_buffer[i];
		}
		if (p_common->p_buffer[i] > max_v) {
			max_p = i;
			max_v = p_common->p_buffer[i];
		}
	}

	sum = 0;
	for (i = 0; i < p_common->size; i++) {
		if (i != min_p && i != max_p) sum += p_common->p_buffer[i];
	}
	p_common->result = sum / (p_common->size - 2);
	return &p_common->result;
}

static void _clearWeightFactor(PifNoiseFilterMethod* p_method)
{
	PifNfInt16WeightFactor* p_owner = (PifNfInt16WeightFactor*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;

	if (p_common->p_buffer) {
		free(p_common->p_buffer);
		p_common->p_buffer = NULL;
	}
	if (p_owner->value) {
		free(p_owner->value);
		p_owner->value = NULL;
	}
}

static void _resetWeightFactor(PifNoiseFilterMethod* p_method)
{
	PifNfInt16WeightFactor* p_owner = (PifNfInt16WeightFactor*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;

	memset(p_common->p_buffer, 0, p_common->size * sizeof(int16_t));
	p_common->current = 0;
	p_owner->total = 0;
}

static PifNoiseFilterValueP _processWeightFactor(PifNoiseFilterMethod* p_method, PifNoiseFilterValueP p_value)
{
	PifNfInt16WeightFactor* p_owner = (PifNfInt16WeightFactor*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;
	int i, n;
	int32_t sum;

	p_common->current = (p_common->current + 1) % p_common->size;
	p_common->p_buffer[p_common->current] = *(int16_t*)p_value;

	sum = 0;
	n = p_common->current;
	for (i = 0; i < p_common->size; i++) {
		sum += p_common->p_buffer[n] * p_owner->value[i];
		n = (n + 1) % p_common->size;
	}
	p_common->result = sum / p_owner->total;
	return &p_common->result;
}

static void _clearNoiseCancel(PifNoiseFilterMethod* p_method)
{
	PifNfInt16NoiseCancel* p_owner = (PifNfInt16NoiseCancel*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;

	if (p_common->p_buffer) {
		free(p_common->p_buffer);
		p_common->p_buffer = NULL;
	}
	if (p_owner->diff) {
		free(p_owner->diff);
		p_owner->diff = NULL;
	}
}

static void _resetNoiseCancel(PifNoiseFilterMethod* p_method)
{
	PifNfInt16NoiseCancel* p_owner = (PifNfInt16NoiseCancel*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;

	memset(p_common->p_buffer, 0, p_common->size * sizeof(int16_t));
	memset(p_owner->diff, 0, p_common->size * 3 * sizeof(int16_t));
	p_common->current = 0;
	p_owner->before = 0;
}

static PifNoiseFilterValueP _processNoiseCancel(PifNoiseFilterMethod* p_method, PifNoiseFilterValueP p_value)
{
	PifNfInt16NoiseCancel* p_owner = (PifNfInt16NoiseCancel*)p_method;
	PifNfInt16Common* p_common = &p_owner->common;
	int i, count;
	int32_t sum;
	int16_t* p_current;
	int16_t* p_before;
	int16_t current[3];

	p_before = p_owner->diff + p_common->current * 3;

	current[0] = *(int16_t*)p_value - p_common->p_buffer[p_common->current];

	current[1] = current[0] - p_before[0];
	if (current[1] < 0) current[1] = -current[1];

	current[2] = current[0] + p_before[0];
	if (current[2] < 0) current[2] = -current[2];

	if (current[1] > current[2]) {
		p_common->p_buffer[p_common->current] = *(int16_t*)p_value;

		p_current = p_before;
		p_before = p_owner->diff + p_owner->before * 3;

		p_current[0] = p_common->p_buffer[p_common->current] - p_common->p_buffer[p_owner->before];

		p_current[1] = p_current[0] - p_before[0];
		if (p_current[1] < 0) p_current[1] = -p_current[1];

		p_current[2] = p_current[0] + p_before[0];
		if (p_current[2] < 0) p_current[2] = -p_current[2];
	}
	else {
		p_owner->before = p_common->current;
		p_common->current = (p_common->current + 1) % p_common->size;
		p_common->p_buffer[p_common->current] = *(int16_t*)p_value;

		p_current = p_owner->diff + p_common->current * 3;

		p_current[0] = current[0];
		p_current[1] = current[1];
		p_current[2] = current[2];
	}

	sum = 0;
	count = 0;
	for (i = 0; i < p_common->size; i++) {
		sum += p_common->p_buffer[i];
		count++;
	}
	p_common->result = count > 0 ? sum / count : 0;
	return &p_common->result;
}

BOOL pifNoiseFilterInt16_AddAverage(PifNoiseFilter* p_parent, uint8_t size)
{
	PifNfInt16Average* p_method;

	if (!_checkParam(p_parent, size != 0)) return FALSE;

	p_parent->__p_method[p_parent->_last] = calloc(1, sizeof(PifNfInt16Average));
	if (!p_parent->__p_method[p_parent->_last]) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_method = (PifNfInt16Average*)p_parent->__p_method[p_parent->_last];

	if (!_allocBuffer(&p_method->common, size)) return FALSE;

	p_method->parent.type = NFT_AVERAGE;
	p_method->parent.fn_clear = _clearAverage;
	p_method->parent.fn_reset = _resetAverage;
	p_method->parent.fn_process = _processAverage;

	p_parent->_last++;
	return TRUE;
}

BOOL pifNoiseFilterInt16_AddWeightFactor(PifNoiseFilter* p_parent, uint8_t size, ...)
{
	PifNfInt16WeightFactor* p_method;
	int i, n;
	va_list ap;

	if (!_checkParam(p_parent, (size & 1) != 0)) return FALSE;

	p_parent->__p_method[p_parent->_last] = calloc(1, sizeof(PifNfInt16WeightFactor));
	if (!p_parent->__p_method[p_parent->_last]) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_method = (PifNfInt16WeightFactor*)p_parent->__p_method[p_parent->_last];

	p_method->value = calloc(size, sizeof(int8_t));
	if (!p_method->value) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	if (!_allocBuffer(&p_method->common, size)) return FALSE;

	va_start(ap, size);
	p_method->total = 0;
	n = size / 2;
	for (i = 0; i < size; i++) {
		p_method->value[n] = va_arg(ap, int);
		p_method->total += p_method->value[n];
		n = (n + 1) % size;
	}
	va_end(ap);

	p_method->parent.type = NFT_WEIGHT_FACTOR;
	p_method->parent.fn_clear = _clearWeightFactor;
	p_method->parent.fn_reset = _resetWeightFactor;
	p_method->parent.fn_process = _processWeightFactor;

	p_parent->_last++;
	return TRUE;
}

BOOL pifNoiseFilterInt16_AddNoiseCancel(PifNoiseFilter* p_parent, uint8_t size)
{
	PifNfInt16NoiseCancel* p_method;

	if (!_checkParam(p_parent, size >= 3 && size <= 32)) return FALSE;

	p_parent->__p_method[p_parent->_last] = calloc(1, sizeof(PifNfInt16NoiseCancel));
	if (!p_parent->__p_method[p_parent->_last]) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_method = (PifNfInt16NoiseCancel*)p_parent->__p_method[p_parent->_last];

	p_method->diff = calloc(size * 3, sizeof(int16_t));
	if (!p_method->diff) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	if (!_allocBuffer(&p_method->common, size)) return FALSE;

	p_method->parent.type = NFT_NOISE_CANCEL;
	p_method->parent.fn_clear = _clearNoiseCancel;
	p_method->parent.fn_reset = _resetNoiseCancel;
	p_method->parent.fn_process = _processNoiseCancel;

	p_parent->_last++;
	return TRUE;
}
