// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_math.h"

#include <float.h>
#include <math.h>

// Fast float approximations and small numeric helpers.
//
// The sin and acos polynomial coefficients come from M. Abramowitz and
// I. A. Stegun, "Handbook of Mathematical Functions" (NBS AMS 55, 1964), a U.S.
// government work. atan, exp and ln use exact series (Taylor and atanh) on a
// reduced range.

// 2*pi split into three parts (Cody-Waite). The first two have their low
// mantissa bits cleared, so n * part is exact and x - n * 2*pi keeps the
// precision of x for |n| < 4096.
#define _2PI_A		6.28125f
#define _2PI_B		1.935005188e-3f
#define _2PI_C		3.019916051e-7f
#define _LOG2_E		1.44269504f
#define _LN_2		0.693147181f
#define _LN_2_HI	0.693145752f		// ln(2) with the low bits cleared, so n * _LN_2_HI is exact
#define _LN_2_LO	1.42860677e-6f		// ln(2) - _LN_2_HI
#define _SQRT_2		1.41421356f
#define _SQRT_3		1.73205081f
#define _TAN_PI_12	0.267949194f		// tan(pi/12) = 2 - sqrt(3)

// -ffast-math lets the compiler fold the split constants below back into one
// (x - n*A - n*B = x - n*(A+B)), which loses the precision they are there for.
// An empty asm statement hides the intermediate value from that rewrite.
#if defined(__GNUC__) && defined(__FAST_MATH__)
#define _KEEP(v)	__asm__ ("" : "+g" (v))
#else
#define _KEEP(v)	((void)0)
#endif

typedef union {
	float f;
	uint32_t u;
} _FloatBits;

static int32_t _roundToInt(float x)
{
	return (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
}

/**
 * @brief Reduces an angle to [-pi, pi].
 * @param x Angle in radians.
 * @return x minus the nearest multiple of 2*pi.
 */
static float _reduceAngle(float x)
{
	float n = (float)_roundToInt(x * (1.0f / (2 * PIF_PI)));

	x -= n * _2PI_A;
	_KEEP(x);
	x -= n * _2PI_B;
	_KEEP(x);
	return x - n * _2PI_C;
}

/**
 * @brief Calculates sin(x) for x in [-3*pi/2, 3*pi/2].
 * @param x Angle in radians.
 * @return Approximation of sin(x).
 */
static float _sinReduced(float x)
{
	float x2;

	// Fold onto [-pi/2, pi/2] where sin is odd and monotonic.
	if (x > PIF_PI / 2) x = PIF_PI - x;
	else if (x < -PIF_PI / 2) x = -PIF_PI - x;

	// A&S 4.3.97: sin(x)/x for 0 <= x <= pi/2, |error| <= 2e-9.
	x2 = x * x;
	return x * (1.0f + x2 * (-0.1666666664f + x2 * (0.0083333315f + x2 * (-0.0001984090f
			+ x2 * (0.0000027526f + x2 * -0.0000000239f)))));
}

float pifMath_SinApprox(float x)
{
	return _sinReduced(_reduceAngle(x));
}

float pifMath_CosApprox(float x)
{
	// Add pi/2 after the reduction so a large x does not round it away.
	return _sinReduced(_reduceAngle(x) + PIF_PI / 2);
}

float pifMath_Atan2Approx(float y, float x)
{
	float ax = fabsf(x), ay = fabsf(y);
	float z, z2, offset, angle;

	if (ax == 0.0f && ay == 0.0f) return 0.0f;

	// Fold onto 0 <= z <= 1, then onto |z| <= tan(pi/12) with
	// atan(z) = pi/6 + atan((sqrt(3) * z - 1) / (sqrt(3) + z)).
	z = ay > ax ? ax / ay : ay / ax;
	offset = 0.0f;
	if (z > _TAN_PI_12) {
		z = (_SQRT_3 * z - 1.0f) / (_SQRT_3 + z);
		offset = PIF_PI / 6;
	}

	// Taylor series of atan to z^9. |z| <= 0.268, so the remainder is below 5e-8.
	z2 = z * z;
	angle = offset + z * (1.0f + z2 * (-1.0f / 3 + z2 * (1.0f / 5 + z2 * (-1.0f / 7 + z2 * (1.0f / 9)))));

	if (ay > ax) angle = PIF_PI / 2 - angle;
	if (x < 0.0f) angle = PIF_PI - angle;
	return y < 0.0f ? -angle : angle;
}

float pifMath_AcosApprox(float x)
{
	float ax, result;

	if (x > 1.0f) x = 1.0f;
	else if (x < -1.0f) x = -1.0f;
	ax = fabsf(x);

	// A&S 4.4.46: acos(x) = sqrt(1 - x) * P(x) for 0 <= x <= 1, |error| <= 2e-8.
	result = sqrtf(1.0f - ax) * (1.5707963050f + ax * (-0.2145988016f + ax * (0.0889789874f
			+ ax * (-0.0501743046f + ax * (0.0308918810f + ax * (-0.0170881256f
			+ ax * (0.0066700901f + ax * -0.0012624911f)))))));
	return x < 0.0f ? PIF_PI - result : result;
}

float pifMath_ExpApprox(float x)
{
	_FloatBits scale;
	float t, g, r;
	int32_t n;

	// e^x = 2^n * e^g with n = round(x / ln2) and |g| <= ln2 / 2. ln2 is split in
	// two parts so that x - n * ln2 keeps the precision of x.
	t = x * _LOG2_E;
	if (t < -126.0f) return 0.0f;
	if (t >= 128.0f) return FLT_MAX;
	n = _roundToInt(t);
	g = x - (float)n * _LN_2_HI;
	_KEEP(g);
	g -= (float)n * _LN_2_LO;

	// Taylor series of e^g to g^6. |g| <= 0.347, so the remainder is below 2e-7.
	r = 1.0f + g * (1.0f + g * (1.0f / 2 + g * (1.0f / 6 + g * (1.0f / 24 + g * (1.0f / 120 + g * (1.0f / 720))))));

	// 2^128 has no float encoding, so take one factor of two into r.
	if (n > 127) {
		r *= 2.0f;
		n--;
	}
	scale.u = (uint32_t)(n + 127) << 23;
	return r * scale.f;
}

float pifMath_LogApprox(float x)
{
	_FloatBits bits;
	int32_t e;
	float m, r, r2;

	if (x <= 0.0f) return -FLT_MAX;

	// Lift subnormals into the normal range first.
	e = 0;
	if (x < FLT_MIN) {
		x *= 8388608.0f;	// 2^23
		e = -23;
	}

	// x = 2^e * m with m in [sqrt(2)/2, sqrt(2)).
	bits.f = x;
	e += (int32_t)((bits.u >> 23) & 0xFF) - 127;
	bits.u = (bits.u & 0x007FFFFF) | 0x3F800000;
	m = bits.f;
	if (m > _SQRT_2) {
		m *= 0.5f;
		e++;
	}

	// ln(m) = 2 * atanh(r) = 2 * (r + r^3/3 + r^5/5 + r^7/7 + ...) with r = (m - 1) / (m + 1).
	// |r| <= 0.172, so the dropped terms are below 3e-8.
	r = (m - 1.0f) / (m + 1.0f);
	r2 = r * r;
	return (float)e * _LN_2 + 2.0f * r * (1.0f + r2 * (1.0f / 3 + r2 * (1.0f / 5 + r2 * (1.0f / 7))));
}

float pifMath_PowApprox(float a, float b)
{
	if (a <= 0.0f) return 0.0f;
	return pifMath_ExpApprox(b * pifMath_LogApprox(a));
}

int32_t pifMath_ScaleRange(int32_t x, int32_t src_from, int32_t src_to, int32_t dst_from, int32_t dst_to)
{
	if (src_to == src_from) return dst_from;
	return (int32_t)((int64_t)(x - src_from) * (dst_to - dst_from) / (src_to - src_from)) + dst_from;
}

float pifMath_ScaleRangeF(float x, float src_from, float src_to, float dst_from, float dst_to)
{
	if (src_to == src_from) return dst_from;
	return (x - src_from) * (dst_to - dst_from) / (src_to - src_from) + dst_from;
}

int32_t pifMath_Deadband(int32_t value, int32_t deadband)
{
	if (value > deadband) return value - deadband;
	if (value < -deadband) return value + deadband;
	return 0;
}

float pifMath_DeadbandF(float value, float deadband)
{
	if (value > deadband) return value - deadband;
	if (value < -deadband) return value + deadband;
	return 0.0f;
}

int32_t pifMath_MedianInt32(const int32_t* p_values, uint8_t count)
{
	int32_t sorted[PIF_MATH_MEDIAN_MAX], value;
	uint8_t i, j;

	if (count == 0 || count > PIF_MATH_MEDIAN_MAX) return 0;

	// Insertion sort into a copy; count is small.
	for (i = 0; i < count; i++) {
		value = p_values[i];
		for (j = i; j > 0 && sorted[j - 1] > value; j--) {
			sorted[j] = sorted[j - 1];
		}
		sorted[j] = value;
	}
	return sorted[count / 2];
}

float pifMath_MedianFloat(const float* p_values, uint8_t count)
{
	float sorted[PIF_MATH_MEDIAN_MAX], value;
	uint8_t i, j;

	if (count == 0 || count > PIF_MATH_MEDIAN_MAX) return 0.0f;

	for (i = 0; i < count; i++) {
		value = p_values[i];
		for (j = i; j > 0 && sorted[j - 1] > value; j--) {
			sorted[j] = sorted[j - 1];
		}
		sorted[j] = value;
	}
	return sorted[count / 2];
}
