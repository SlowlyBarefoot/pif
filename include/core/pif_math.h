// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MATH_H
#define PIF_MATH_H


#include "core/pif.h"


// Largest count accepted by pifMath_MedianInt32() and pifMath_MedianFloat().
#define PIF_MATH_MEDIAN_MAX		15


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMath_SinApprox
 * @brief Calculates sin(x) with a polynomial, without the C library.
 *
 * Uses Abramowitz and Stegun 4.3.97 after reducing x to [-pi/2, pi/2].
 * The error is below 1e-6 for |x| up to about 25000 rad.
 * @param x Angle in radians.
 * @return Approximation of sin(x).
 */
float pifMath_SinApprox(float x);

/**
 * @fn pifMath_CosApprox
 * @brief Calculates cos(x) as sin(x + pi/2) with the polynomial of pifMath_SinApprox().
 * @param x Angle in radians.
 * @return Approximation of cos(x).
 */
float pifMath_CosApprox(float x);

/**
 * @fn pifMath_Atan2Approx
 * @brief Calculates atan2(y, x) with a polynomial, without the C library.
 *
 * Folds the angle onto |z| <= tan(pi/12) and sums the Taylor series of atan.
 * The error is below 5e-7 rad. atan2(0, 0) returns 0.
 * @param y Y coordinate.
 * @param x X coordinate.
 * @return Angle in radians in [-pi, pi].
 */
float pifMath_Atan2Approx(float y, float x);

/**
 * @fn pifMath_AcosApprox
 * @brief Calculates acos(x) with a polynomial, without the C library's acos.
 *
 * Uses Abramowitz and Stegun 4.4.46. The error is below 1e-6 rad.
 * x is clamped to [-1, 1].
 * @param x Cosine value.
 * @return Angle in radians in [0, pi].
 */
float pifMath_AcosApprox(float x);

/**
 * @fn pifMath_ExpApprox
 * @brief Calculates e^x by splitting x into a power of two and a short Taylor series.
 *
 * The relative error is below 1e-6. Results below FLT_MIN return 0 and results
 * above FLT_MAX return the largest finite float.
 * @param x Exponent.
 * @return Approximation of e^x.
 */
float pifMath_ExpApprox(float x);

/**
 * @fn pifMath_LogApprox
 * @brief Calculates ln(x) from the float exponent and an atanh series of the mantissa.
 *
 * The error is below 1e-6 * max(1, |ln(x)|). x <= 0 returns -FLT_MAX.
 * @param x Positive value.
 * @return Approximation of ln(x).
 */
float pifMath_LogApprox(float x);

/**
 * @fn pifMath_PowApprox
 * @brief Calculates a^b as e^(b * ln(a)) with pifMath_ExpApprox() and pifMath_LogApprox().
 * @param a Positive base. a <= 0 returns 0.
 * @param b Exponent.
 * @return Approximation of a^b.
 */
float pifMath_PowApprox(float a, float b);

/**
 * @fn pifMath_ScaleRange
 * @brief Maps x linearly from [src_from, src_to] to [dst_from, dst_to] with integer arithmetic.
 *
 * x is not clamped, so values outside the source range map outside the destination range.
 * @param x Input value.
 * @param src_from Source value that maps to dst_from.
 * @param src_to Source value that maps to dst_to. Must differ from src_from.
 * @param dst_from Destination value for src_from.
 * @param dst_to Destination value for src_to.
 * @return Mapped value, rounded toward zero.
 */
int32_t pifMath_ScaleRange(int32_t x, int32_t src_from, int32_t src_to, int32_t dst_from, int32_t dst_to);

/**
 * @fn pifMath_ScaleRangeF
 * @brief Maps x linearly from [src_from, src_to] to [dst_from, dst_to].
 * @param x Input value.
 * @param src_from Source value that maps to dst_from.
 * @param src_to Source value that maps to dst_to. Must differ from src_from.
 * @param dst_from Destination value for src_from.
 * @param dst_to Destination value for src_to.
 * @return Mapped value.
 */
float pifMath_ScaleRangeF(float x, float src_from, float src_to, float dst_from, float dst_to);

/**
 * @fn pifMath_Deadband
 * @brief Zeroes values within +-deadband and shifts the rest toward zero by deadband.
 * @param value Input value.
 * @param deadband Non-negative half width of the dead zone.
 * @return Value with the dead zone removed.
 */
int32_t pifMath_Deadband(int32_t value, int32_t deadband);

/**
 * @fn pifMath_DeadbandF
 * @brief Float version of pifMath_Deadband().
 * @param value Input value.
 * @param deadband Non-negative half width of the dead zone.
 * @return Value with the dead zone removed.
 */
float pifMath_DeadbandF(float value, float deadband);

/**
 * @fn pifMath_MedianInt32
 * @brief Returns the median of count values without changing them.
 *
 * For an even count the upper of the two middle values is returned.
 * @param p_values Pointer to the values.
 * @param count Number of values, 1 to PIF_MATH_MEDIAN_MAX.
 * @return Median, or 0 if count is out of range.
 */
int32_t pifMath_MedianInt32(const int32_t* p_values, uint8_t count);

/**
 * @fn pifMath_MedianFloat
 * @brief Float version of pifMath_MedianInt32().
 * @param p_values Pointer to the values.
 * @param count Number of values, 1 to PIF_MATH_MEDIAN_MAX.
 * @return Median, or 0 if count is out of range.
 */
float pifMath_MedianFloat(const float* p_values, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MATH_H
