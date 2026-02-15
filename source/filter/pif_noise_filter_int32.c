#include "filter/pif_noise_filter_int32.h"


/**
 * @fn _allocBuffer
 * @brief Allocates the sample buffer used by a 32-bit filter.
 * @param p_common Pointer to the shared 32-bit filter state.
 * @param size Number of samples to allocate.
 * @return TRUE if allocation succeeds, otherwise FALSE.
 */
static BOOL _allocBuffer(PifNfInt32Common* p_common, uint8_t size)
{
	p_common->p_buffer = calloc(size, sizeof(int32_t));
	if (!p_common->p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_common->size = size;
	return TRUE;
}

/**
 * @fn _clearAverage
 * @brief Releases resources owned by a 32-bit trimmed-average filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _clearAverage(PifNoiseFilter* p_parent)
{
	PifNfInt32Common* p_common = &((PifNfInt32Average*)p_parent)->common;

	if (p_common->p_buffer) {
		free(p_common->p_buffer);
		p_common->p_buffer = NULL;
	}
}

/**
 * @fn _resetAverage
 * @brief Resets the runtime state of a 32-bit trimmed-average filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetAverage(PifNoiseFilter* p_parent)
{
	PifNfInt32Average* p_owner = (PifNfInt32Average*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;

	memset(p_common->p_buffer, 0, p_common->size * sizeof(int32_t));
	p_common->current = 0;
	p_owner->len = 0;
}

/**
 * @fn _processAverage
 * @brief Processes a sample using trimmed average (excluding min and max).
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest int32_t input sample.
 * @return Pointer to the filtered int32_t result, or NULL until the window is full.
 */
static PifNoiseFilterValueP _processAverage(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNfInt32Average* p_owner = (PifNfInt32Average*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;
	uint8_t min_p, max_p, i;
	int32_t min_v, max_v;
	int32_t sum;

	p_common->p_buffer[p_common->current] = *(int32_t*)p_value;
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

/**
 * @fn _clearWeightFactor
 * @brief Releases resources owned by a 32-bit weighted filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _clearWeightFactor(PifNoiseFilter* p_parent)
{
	PifNfInt32WeightFactor* p_owner = (PifNfInt32WeightFactor*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;

	if (p_common->p_buffer) {
		free(p_common->p_buffer);
		p_common->p_buffer = NULL;
	}
	if (p_owner->value) {
		free(p_owner->value);
		p_owner->value = NULL;
	}
}

/**
 * @fn _resetWeightFactor
 * @brief Resets the runtime state of a 32-bit weighted filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetWeightFactor(PifNoiseFilter* p_parent)
{
	PifNfInt32WeightFactor* p_owner = (PifNfInt32WeightFactor*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;

	memset(p_common->p_buffer, 0, p_common->size * sizeof(int32_t));
	p_common->current = 0;
	p_owner->total = 0;
}

/**
 * @fn _processWeightFactor
 * @brief Processes a sample using weighted moving average.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest int32_t input sample.
 * @return Pointer to the filtered int32_t result.
 */
static PifNoiseFilterValueP _processWeightFactor(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNfInt32WeightFactor* p_owner = (PifNfInt32WeightFactor*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;
	int i, n;
	int32_t sum;

	p_common->current = (p_common->current + 1) % p_common->size;
	p_common->p_buffer[p_common->current] = *(int32_t*)p_value;

	sum = 0;
	n = p_common->current;
	for (i = 0; i < p_common->size; i++) {
		sum += p_common->p_buffer[n] * p_owner->value[i];
		n = (n + 1) % p_common->size;
	}
	p_common->result = sum / p_owner->total;
	return &p_common->result;
}

/**
 * @fn _clearNoiseCancel
 * @brief Releases resources owned by a 32-bit noise-cancel filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _clearNoiseCancel(PifNoiseFilter* p_parent)
{
	PifNfInt32NoiseCancel* p_owner = (PifNfInt32NoiseCancel*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;

	if (p_common->p_buffer) {
		free(p_common->p_buffer);
		p_common->p_buffer = NULL;
	}
	if (p_owner->diff) {
		free(p_owner->diff);
		p_owner->diff = NULL;
	}
}

/**
 * @fn _resetNoiseCancel
 * @brief Resets the runtime state of a 32-bit noise-cancel filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetNoiseCancel(PifNoiseFilter* p_parent)
{
	PifNfInt32NoiseCancel* p_owner = (PifNfInt32NoiseCancel*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;

	memset(p_common->p_buffer, 0, p_common->size * sizeof(int32_t));
	memset(p_owner->diff, 0, p_common->size * 3 * sizeof(int16_t));
	p_common->current = 0;
	p_owner->before = 0;
}

/**
 * @fn _processNoiseCancel
 * @brief Processes a sample using adaptive difference-based noise cancellation.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest int32_t input sample.
 * @return Pointer to the filtered int32_t result.
 */
static PifNoiseFilterValueP _processNoiseCancel(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNfInt32NoiseCancel* p_owner = (PifNfInt32NoiseCancel*)p_parent;
	PifNfInt32Common* p_common = &p_owner->common;
	int i, count;
	int32_t sum;
	int16_t* p_current;
	int16_t* p_before;
	int16_t current[3];

	p_before = p_owner->diff + p_common->current * 3;

	current[0] = *(int32_t*)p_value - p_common->p_buffer[p_common->current];

	current[1] = current[0] - p_before[0];
	if (current[1] < 0) current[1] = -current[1];

	current[2] = current[0] + p_before[0];
	if (current[2] < 0) current[2] = -current[2];

	if (current[1] > current[2]) {
		p_common->p_buffer[p_common->current] = *(int32_t*)p_value;

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
		p_common->p_buffer[p_common->current] = *(int32_t*)p_value;

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

PifNoiseFilter* pifNoiseFilterInt32_AddAverage(PifNoiseFilterManager* p_manager, uint8_t size)
{
	PifNfInt32Average* p_owner;
	PifPtrArrayIterator it;

	if (!p_manager || !size) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	p_owner = calloc(1, sizeof(PifNfInt32Average));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	it = pifPtrArray_Add(&p_manager->__filters, p_owner);
	if (!it) goto fail;

	if (!_allocBuffer(&p_owner->common, size)) goto fail;

	p_owner->parent._type = NFT_AVERAGE;
	p_owner->parent.__fn_clear = _clearAverage;
	p_owner->parent.__fn_reset = _resetAverage;
	p_owner->parent.__fn_process = _processAverage;
	return (PifNoiseFilter*)p_owner;

fail:
	if (it) pifPtrArray_Remove(&p_manager->__filters, it);
	if (p_owner) free(p_owner);
	return NULL;
}

PifNoiseFilter* pifNoiseFilterInt32_AddWeightFactor(PifNoiseFilterManager* p_manager, uint8_t size, ...)
{
	PifNfInt32WeightFactor* p_owner;
	PifPtrArrayIterator it = NULL;
	int i, n;
	va_list ap;

	if (!p_manager || !(size & 1)) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	p_owner = calloc(1, sizeof(PifNfInt32WeightFactor));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	p_owner->value = calloc(size, sizeof(int8_t));
	if (!p_owner->value) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	it = pifPtrArray_Add(&p_manager->__filters, p_owner);
	if (!it) goto fail;

	if (!_allocBuffer(&p_owner->common, size)) goto fail;

	va_start(ap, size);
	p_owner->total = 0;
	n = size / 2;
	for (i = 0; i < size; i++) {
		p_owner->value[n] = va_arg(ap, int);
		p_owner->total += p_owner->value[n];
		n = (n + 1) % size;
	}
	va_end(ap);

	p_owner->parent._type = NFT_WEIGHT_FACTOR;
	p_owner->parent.__fn_clear = _clearWeightFactor;
	p_owner->parent.__fn_reset = _resetWeightFactor;
	p_owner->parent.__fn_process = _processWeightFactor;
	return (PifNoiseFilter*)p_owner;

fail:
	if (it) pifPtrArray_Remove(&p_manager->__filters, it);
	if (p_owner) {
		if (p_owner->value) free(p_owner->value);
		free(p_owner);
	}
	return NULL;
}

PifNoiseFilter* pifNoiseFilterInt32_AddNoiseCancel(PifNoiseFilterManager* p_manager, uint8_t size)
{
	PifNfInt32NoiseCancel* p_owner;
	PifPtrArrayIterator it = NULL;

	if (!p_manager || size < 3 || size > 32) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	p_owner = calloc(1, sizeof(PifNfInt32NoiseCancel));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	p_owner->diff = calloc(size * 3, sizeof(int16_t));
	if (!p_owner->diff) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	it = pifPtrArray_Add(&p_manager->__filters, p_owner);
	if (!it) goto fail;

	if (!_allocBuffer(&p_owner->common, size)) goto fail;

	p_owner->parent._type = NFT_NOISE_CANCEL;
	p_owner->parent.__fn_clear = _clearNoiseCancel;
	p_owner->parent.__fn_reset = _resetNoiseCancel;
	p_owner->parent.__fn_process = _processNoiseCancel;
	return (PifNoiseFilter*)p_owner;

fail:
	if (it) pifPtrArray_Remove(&p_manager->__filters, it);
	if (p_owner) {
		if (p_owner->diff) free(p_owner->diff);
		free(p_owner);
	}
	return NULL;
}
