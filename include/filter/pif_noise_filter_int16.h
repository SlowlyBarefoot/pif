// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NOISE_FILTER_INT16_H
#define PIF_NOISE_FILTER_INT16_H


// Declares PifNfInt16Average, PifNfInt16Median, PifNfInt16Ema and PifNfInt16Deadband with their Init functions
// on int16_t samples. See pif_noise_filter_int.h for them.
#define PIF_NF_INT_BITS		16
#define PIF_NF_INT_WIDE_T	int32_t
#include "filter/pif_noise_filter_int.h"
#undef PIF_NF_INT_BITS
#undef PIF_NF_INT_WIDE_T


#endif  // PIF_NOISE_FILTER_INT16_H
