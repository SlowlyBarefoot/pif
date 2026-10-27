// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_marker_domain.h
 * @brief Marker domains: the upper 16 bits of a marker code, which say whose code it is.
 *
 * A marker code says what happened, as a compile-time constant made with PIF_MARKER_CODE(domain, local).
 * Who it happened to is the PifId given beside it, so a code never holds a PifId, whose PIF_ID_AUTO
 * values change from build to build.
 *
 * Numbers are only ever appended here. A number once used is never removed or given to another owner, so
 * a code means the same thing in every firmware version and field reports can be added up across them. A
 * PIF module gets its number when it first puts a marker, and keeps its local codes, the lower 16 bits,
 * as an enum in its own header, only appended too.
 */
#ifndef PIF_MARKER_DOMAIN_H
#define PIF_MARKER_DOMAIN_H


/// The recorder itself: 0 text, 1 rec.drop, 2 rec.overflow.
#define PIF_MARKER_DOMAIN_RECORDER		0x0000

/// PIF modules, numbered in the order they first put a marker. None does yet.
#define PIF_MARKER_DOMAIN_MODULE_FIRST	0x0001
#define PIF_MARKER_DOMAIN_MODULE_LAST	0x7FFF

/// Applications and products, free for each of them.
#define PIF_MARKER_DOMAIN_APP_FIRST		0x8000
#define PIF_MARKER_DOMAIN_APP_LAST		0xFFFF


#endif	// PIF_MARKER_DOMAIN_H
