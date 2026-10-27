// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_marker.h
 * @brief Markers: moments worth keeping in a log, such as an error or the start of a step.
 *
 * Code that notices something puts a marker with pifMarker_Put(). Nothing is kept until a recorder
 * attaches itself to pif_act_marker; until then a marker costs one test of a NULL pointer.
 *
 *     #define UART_MK_OVERRUN		PIF_MARKER_CODE(PIF_MARKER_DOMAIN_..., 1)
 *
 *     int32_t lost = 3;
 *     pifMarker_Put(UART_MK_OVERRUN, p_owner->_id, &lost, 1);
 *
 * pifMarker_Put() may be called from an interrupt, so whatever is attached must allow that.
 */
#ifndef PIF_MARKER_H
#define PIF_MARKER_H


#include "core/pif.h"
#include "core/pif_marker_domain.h"


/// Code of a marker: the domain of its owner (pif_marker_domain.h) and the owner's local code.
#define PIF_MARKER_CODE(DOMAIN, LOCAL)	(((uint32_t)(DOMAIN) << 16) | (uint16_t)(LOCAL))

/**
 * @brief Takes a marker.
 * @param code PIF_MARKER_CODE() of what happened.
 * @param src PifId of the object it happened to, or 0 for none.
 * @param p_args Arguments of the marker, or NULL when argc is 0.
 * @param argc Number of arguments.
 */
typedef void (*PifActMarker)(uint32_t code, PifId src, const int32_t *p_args, uint8_t argc);


#ifdef __cplusplus
extern "C" {
#endif

/// Where markers go, set by a recorder. NULL, the default, drops them.
extern PifActMarker pif_act_marker;

#ifdef __cplusplus
}
#endif


/// Puts a marker to pif_act_marker when it is set.
#define pifMarker_Put(CODE, SRC, P_ARGS, ARGC) \
	do { if (pif_act_marker) (*pif_act_marker)((CODE), (SRC), (P_ARGS), (ARGC)); } while (0)


#endif	// PIF_MARKER_H
