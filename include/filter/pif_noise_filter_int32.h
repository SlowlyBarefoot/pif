// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NOISE_FILTER_INT32_H
#define PIF_NOISE_FILTER_INT32_H


// Declares PifNfInt32Average, PifNfInt32Median, PifNfInt32Ema and PifNfInt32Deadband with their Init functions
// on int32_t samples. See pif_noise_filter_int.h for them.
#define PIF_NF_INT_BITS		32
#define PIF_NF_INT_WIDE_T	int64_t
#include "filter/pif_noise_filter_int.h"
#undef PIF_NF_INT_BITS
#undef PIF_NF_INT_WIDE_T


#endif  // PIF_NOISE_FILTER_INT32_H
