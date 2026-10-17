// SPDX-License-Identifier: BSD-3-Clause
// Template of the integer noise filters, shared by pif_noise_filter_int16.c and pif_noise_filter_int32.c. Each of
// them includes it after its header, with PIF_NF_INT_BITS set to its width and PIF_NF_INT_WIDE_T to a type twice
// as wide.

#if !defined(PIF_NF_INT_BITS) || !defined(PIF_NF_INT_WIDE_T)
	#error "Only pif_noise_filter_int16.c and pif_noise_filter_int32.c include this."
#endif


#define PIF_NF_INT_CAT_(A, B, C)	A##B##C
#define PIF_NF_INT_CAT(A, B, C)		PIF_NF_INT_CAT_(A, B, C)

#define PIF_NF_INT_T				PIF_NF_INT_CAT(int, PIF_NF_INT_BITS, _t)
#define PIF_NF_INT_AVERAGE			PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Average)
#define PIF_NF_INT_MEDIAN			PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Median)
#define PIF_NF_INT_EMA				PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Ema)
#define PIF_NF_INT_DEADBAND			PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Deadband)
#define PIF_NF_INT_VALUE			PIF_NF_INT_CAT(NFV_INT, PIF_NF_INT_BITS, )


/**
 * @fn _resetAverage
 * @brief Empties the window of a trimmed-average filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetAverage(PifNoiseFilter* p_parent)
{
	PIF_NF_INT_AVERAGE* p_owner = (PIF_NF_INT_AVERAGE*)p_parent;

	p_parent->_ready = FALSE;
	p_owner->_result = 0;
	p_owner->__current = 0;
	p_owner->__len = 0;
}

/**
 * @fn _processAverage
 * @brief Processes a sample using trimmed average (excluding min and max).
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest input sample.
 * @return Pointer to the filtered result.
 */
static PifNoiseFilterValueP _processAverage(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PIF_NF_INT_AVERAGE* p_owner = (PIF_NF_INT_AVERAGE*)p_parent;
	PIF_NF_INT_T* p_buffer = p_owner->__p_buffer;
	uint8_t i;
	PIF_NF_INT_T value, min_v, max_v;
	PIF_NF_INT_WIDE_T sum;

	p_buffer[p_owner->__current] = *(PIF_NF_INT_T*)p_value;
	p_owner->__current = (p_owner->__current + 1) % p_owner->_size;
	if (p_owner->__len < p_owner->_size && ++p_owner->__len == p_owner->_size) p_parent->_ready = TRUE;

	// Until the window is full, the samples are in its first __len places. Taking one minimum and one maximum
	// out of the sum also holds when every sample is the same.
	sum = min_v = max_v = p_buffer[0];
	for (i = 1; i < p_owner->__len; i++) {
		value = p_buffer[i];
		sum += value;
		if (value < min_v) min_v = value;
		else if (value > max_v) max_v = value;
	}
	if (p_owner->__len >= 3) {
		p_owner->_result = (PIF_NF_INT_T)((sum - min_v - max_v) / (p_owner->__len - 2));
	}
	else {
		p_owner->_result = (PIF_NF_INT_T)(sum / p_owner->__len);
	}
	return &p_owner->_result;
}

/**
 * @fn _resetMedian
 * @brief Empties the window of a moving median filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetMedian(PifNoiseFilter* p_parent)
{
	PIF_NF_INT_MEDIAN* p_owner = (PIF_NF_INT_MEDIAN*)p_parent;

	p_parent->_ready = FALSE;
	p_owner->_result = 0;
	p_owner->__current = 0;
	p_owner->__len = 0;
}

/**
 * @fn _processMedian
 * @brief Processes a sample using the median of the window.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest input sample.
 * @return Pointer to the filtered result.
 */
static PifNoiseFilterValueP _processMedian(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PIF_NF_INT_MEDIAN* p_owner = (PIF_NF_INT_MEDIAN*)p_parent;
	PIF_NF_INT_T* p_window = p_owner->__p_buffer;
	PIF_NF_INT_T* p_sorted = p_owner->__p_buffer + p_owner->_size;
	PIF_NF_INT_T value = *(PIF_NF_INT_T*)p_value;
	int i, n;

	if (p_owner->__len == p_owner->_size) {
		// The sample that leaves the window leaves the sorted list too.
		for (i = 0; p_sorted[i] != p_window[p_owner->__current]; i++);
		for (; i < p_owner->_size - 1; i++) p_sorted[i] = p_sorted[i + 1];
		n = p_owner->_size - 1;
	}
	else {
		n = p_owner->__len++;
		if (p_owner->__len == p_owner->_size) p_parent->_ready = TRUE;
	}
	p_window[p_owner->__current] = value;
	p_owner->__current = (p_owner->__current + 1) % p_owner->_size;

	for (i = n; i > 0 && p_sorted[i - 1] > value; i--) p_sorted[i] = p_sorted[i - 1];
	p_sorted[i] = value;

	n = p_owner->__len;
	if (n & 1) {
		p_owner->_result = p_sorted[n / 2];
	}
	else {
		p_owner->_result = (PIF_NF_INT_T)(((PIF_NF_INT_WIDE_T)p_sorted[n / 2 - 1] + p_sorted[n / 2]) / 2);
	}
	return &p_owner->_result;
}

/**
 * @fn _resetEma
 * @brief Forgets the history of an exponential moving average filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetEma(PifNoiseFilter* p_parent)
{
	PIF_NF_INT_EMA* p_owner = (PIF_NF_INT_EMA*)p_parent;

	p_parent->_ready = FALSE;
	p_owner->_result = 0;
	p_owner->__acc = 0;
}

/**
 * @fn _shiftRound
 * @brief Divides by 2^shift, rounding half away from zero, so that negative levels behave as positive ones do.
 * @param value Value to divide.
 * @param shift Power of two to divide by, 1 or more.
 * @return The quotient.
 */
static PIF_NF_INT_T _shiftRound(PIF_NF_INT_WIDE_T value, uint8_t shift)
{
	PIF_NF_INT_WIDE_T half = (PIF_NF_INT_WIDE_T)1 << (shift - 1);

	if (value >= 0) return (PIF_NF_INT_T)((value + half) >> shift);
	return (PIF_NF_INT_T)-((-value + half) >> shift);
}

/**
 * @fn _processEma
 * @brief Processes a sample using exponential moving average.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest input sample.
 * @return Pointer to the filtered result.
 */
static PifNoiseFilterValueP _processEma(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PIF_NF_INT_EMA* p_owner = (PIF_NF_INT_EMA*)p_parent;
	PIF_NF_INT_T value = *(PIF_NF_INT_T*)p_value;

	if (!p_parent->_ready) {
		p_owner->__acc = (PIF_NF_INT_WIDE_T)value * ((PIF_NF_INT_WIDE_T)1 << p_owner->_shift);
		p_parent->_ready = TRUE;
	}
	else {
		// result += (value - result) / 2^_shift, kept 2^_shift times larger.
		p_owner->__acc += (PIF_NF_INT_WIDE_T)value - p_owner->_result;
	}
	p_owner->_result = _shiftRound(p_owner->__acc, p_owner->_shift);
	return &p_owner->_result;
}

/**
 * @fn _resetDeadband
 * @brief Forgets the last result of a deadband filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetDeadband(PifNoiseFilter* p_parent)
{
	p_parent->_ready = FALSE;
	((PIF_NF_INT_DEADBAND*)p_parent)->_result = 0;
}

/**
 * @fn _processDeadband
 * @brief Passes a sample only when it is the band or more away from the result.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest input sample.
 * @return Pointer to the filtered result.
 */
static PifNoiseFilterValueP _processDeadband(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PIF_NF_INT_DEADBAND* p_owner = (PIF_NF_INT_DEADBAND*)p_parent;
	PIF_NF_INT_T value = *(PIF_NF_INT_T*)p_value;
	PIF_NF_INT_WIDE_T diff = (PIF_NF_INT_WIDE_T)value - p_owner->_result;

	if (!p_parent->_ready || diff >= p_owner->_band || diff <= -(PIF_NF_INT_WIDE_T)p_owner->_band) {
		p_owner->_result = value;
		p_parent->_ready = TRUE;
	}
	return &p_owner->_result;
}

BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitAverage)(PIF_NF_INT_AVERAGE* p_owner,
		PIF_NF_INT_T* p_buffer, uint8_t size)
{
	if (!p_owner || !p_buffer || size < 3) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PIF_NF_INT_AVERAGE));
	p_owner->parent._type = NFT_AVERAGE;
	p_owner->parent._value = PIF_NF_INT_VALUE;
	p_owner->parent.__fn_reset = _resetAverage;
	p_owner->parent.__fn_process = _processAverage;
	p_owner->_size = size;
	p_owner->__p_buffer = p_buffer;
	return TRUE;
}

BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitMedian)(PIF_NF_INT_MEDIAN* p_owner,
		PIF_NF_INT_T* p_buffer, uint8_t size)
{
	if (!p_owner || !p_buffer || size < 3) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PIF_NF_INT_MEDIAN));
	p_owner->parent._type = NFT_MEDIAN;
	p_owner->parent._value = PIF_NF_INT_VALUE;
	p_owner->parent.__fn_reset = _resetMedian;
	p_owner->parent.__fn_process = _processMedian;
	p_owner->_size = size;
	p_owner->__p_buffer = p_buffer;
	return TRUE;
}

BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitEma)(PIF_NF_INT_EMA* p_owner, uint8_t shift)
{
	if (!p_owner || shift < 1 || shift >= PIF_NF_INT_BITS) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PIF_NF_INT_EMA));
	p_owner->parent._type = NFT_EMA;
	p_owner->parent._value = PIF_NF_INT_VALUE;
	p_owner->parent.__fn_reset = _resetEma;
	p_owner->parent.__fn_process = _processEma;
	p_owner->_shift = shift;
	return TRUE;
}

BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitDeadband)(PIF_NF_INT_DEADBAND* p_owner,
		PIF_NF_INT_T band)
{
	if (!p_owner || band < 1) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PIF_NF_INT_DEADBAND));
	p_owner->parent._type = NFT_DEADBAND;
	p_owner->parent._value = PIF_NF_INT_VALUE;
	p_owner->parent.__fn_reset = _resetDeadband;
	p_owner->parent.__fn_process = _processDeadband;
	p_owner->_band = band;
	return TRUE;
}
