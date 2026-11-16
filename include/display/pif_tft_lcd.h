// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_tft_lcd.h
 * @brief Base of the TFT LCD drivers: the bus to the controller, the rotation and the backlight.
 *
 * A controller driver such as pif_ili9341 or pif_ssd1963 embeds a PifTftLcd as its first member and fills in the
 * drawing functions of its PifGraphics. The application draws through the pifGraphics functions on
 * &lcd.parent.parent, which pifIli9341_Graphics() and pifSsd1963_Graphics() hand over, so the same code works
 * with every controller and on the other graphic displays of PIF.
 */
#ifndef PIF_TFT_LCD_H
#define PIF_TFT_LCD_H


#include "display/pif_graphics.h"


#define TFT_SETUP_DELAY_MS				0xFF	///< Command code in a setup table that means a delay instead of a command


/**
 * @brief Command code of a controller.
 */
typedef uint32_t PifTftLcdCmd;

/**
 * @brief Rotation of the display.
 */
typedef enum EnPifTftLcdRotation
{
	TLR_0_DEGREE	= 0,	///< Not rotated
	TLR_90_DEGREE,	///< 90 degrees clockwise
	TLR_180_DEGREE,	///< 180 degrees
	TLR_270_DEGREE	///< 270 degrees clockwise
} PifTftLcdRotation;


struct StPifTftLcd;
typedef struct StPifTftLcd PifTftLcd;

/**
 * @brief Resets the controller by its reset pin.
 */
typedef void (*PifActLcdReset)(void);
/**
 * @brief Drives the chip select of the controller.
 */
typedef void (*PifActLcdChipSelect)(SWITCH sw);
/**
 * @brief Writes a command and reads size words of its answer into p_data.
 */
typedef void (*PifActLcdReadCmd)(PifTftLcdCmd cmd, uint32_t* p_data, uint32_t size);
/**
 * @brief Writes a command with size words of parameters from p_data.
 */
typedef void (*PifActLcdWriteCmd)(PifTftLcdCmd cmd, uint32_t* p_data, uint32_t size);
/**
 * @brief Writes size words of pixel data.
 */
typedef void (*PifActLcdWriteData)(uint32_t* p_data, uint32_t size);
/**
 * @brief Writes the size words of one pixel len times, to fill an area.
 */
typedef void (*PifActLcdWriteRepeat)(uint32_t* p_data, uint8_t size, uint32_t len);
/**
 * @brief Sets the brightness of the backlight.
 */
typedef void (*PifActLcdBackLight)(uint8_t level);

/**
 * @brief Changes the rotation. Implemented by the controller driver.
 */
typedef BOOL (*PifTftLcdSetRotation)(PifTftLcd* p_parent, PifTftLcdRotation rotation);

/**
 * @class StPifTftLcd
 * @brief Base TFT LCD driver context shared by controller-specific implementations.
 */
struct StPifTftLcd
{
	/// Drawing surface of the panel. The parent variable must be at the beginning of this structure.
	PifGraphics parent;

	// Public Member Variable

	// Read-only Member Variable
    PifTftLcdRotation _rotation;	///< Current rotation

	// Private Member Variable

	// Read-only Function
    PifTftLcdSetRotation _fn_set_rotation;	///< Set by the controller driver

	// Public Action Function
    PifActLcdBackLight act_backlight;	///< Sets the backlight. May be NULL.

	// Private Action Function
    PifActLcdReset __act_reset;
    PifActLcdChipSelect __act_chip_select;
    PifActLcdReadCmd __act_read_cmd;
    PifActLcdWriteCmd __act_write_cmd;
    PifActLcdWriteData __act_write_data;
    PifActLcdWriteRepeat __act_write_repeat;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the common TFT LCD context with geometry and initial rotation.
 * @param p_owner Pointer to the TFT LCD instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param width Native panel width in pixels.
 * @param height Native panel height in pixels.
 * @param rotation Rotation the panel is in. Only recorded, with width and height swapped for 90 and
 *        270 degrees; the controller itself is set by the setup stream or the set-rotation function.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTftLcd_Init(PifTftLcd* p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation);

/**
 * @brief Hands over the drawing surface of the panel, which the pifGraphics functions draw on.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @return The PifGraphics of the panel.
 */
PIF_STATIC_INLINE PifGraphics* pifTftLcd_Graphics(PifTftLcd* p_owner) { return &p_owner->parent; }

#ifdef __cplusplus
}
#endif


#endif  // PIF_TFT_LCD_H
