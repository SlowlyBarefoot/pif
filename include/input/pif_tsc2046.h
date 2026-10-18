// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_tsc2046.h
 * @brief TSC2046 (ADS7846) resistive touch controller on SPI, as a PifTouchScreen.
 */
#ifndef PIF_TSC2046_H
#define PIF_TSC2046_H


#include "communication/pif_spi.h"
#include "input/pif_touch_screen.h"


/**
 * @brief Reads the PENIRQ pin of the controller. Returns TRUE while the panel is pressed.
 */
typedef BOOL (*PifActTsc2046Pen)(void);

/**
 * @class StPifTsc2046
 * @brief TSC2046 touch controller context built on top of `PifTouchScreen`.
 */
typedef struct StPifTsc2046
{
	/// The parent variable must be at the beginning of this structure.
	PifTouchScreen parent;

	// Public Member Variable

	// Public Event Function

	// Read-only Member Variable
	PifDeviceReg *_p_device;	///< SPI device of the controller

	// Private Member Variable

	// Private Action Function
	PifActTsc2046Pen __act_pen;
} PifTsc2046;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a TSC2046 touch controller and binds SPI/pen callbacks. Each reading is one
 *        full-duplex transfer of three bytes, so the port's act_transfer has to clock out p_write
 *        and clock in p_read byte for byte, and max_transfer_size, when set, must allow three.
 * @param p_owner Pointer to the TSC2046 instance to initialize.
 * @param id Instance ID. Use `PIF_ID_AUTO` to assign an ID automatically.
 * @param p_lcd Target LCD descriptor used for coordinate scaling.
 * @param left_x Raw touch value at the left edge.
 * @param right_x Raw touch value at the right edge.
 * @param top_y Raw touch value at the top edge.
 * @param bottom_y Raw touch value at the bottom edge.
 * @param p_port SPI port used to communicate with the TSC2046.
 * @param p_client User SPI client context passed to the SPI device.
 * @param act_pen Callback that returns `TRUE` while the panel is pressed (PENIRQ low).
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTsc2046_Init(PifTsc2046* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y, PifSpiPort* p_port, void *p_client, PifActTsc2046Pen act_pen);

/**
 * @brief Removes the SPI device associated with the TSC2046 instance.
 * @param p_owner Pointer to the TSC2046 instance.
 */
void pifTsc2046_Clear(PifTsc2046* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_TSC2046_H
