// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_tft_lcd.h
 * @brief Base of the TFT LCD drivers: colors, rotation and the drawing functions.
 *
 * A controller driver such as pif_ili9341 or pif_ssd1963 embeds a PifTftLcd as its first member and fills in its
 * drawing functions. The application draws through the pifTftLcd functions only, so the same code works with
 * every controller.
 */
#ifndef PIF_TFT_LCD_H
#define PIF_TFT_LCD_H


#include "core/pif.h"


#define PIF_COLOR_MAKE16(r8, g8, b8) 	((((b8) & 0xF8) >> 3) + (((g8) & 0xFC) << 3) + (((r8) & 0xF8) << 8))	///< 16 bit RGB 565 color from 8 bit red, green and blue
#define PIF_COLOR_MAKE32(r8, g8, b8) 	((b8) + ((g8) << 8) + ((uint32_t)(r8) << 16))	///< 32 bit RGB 888 color from 8 bit red, green and blue

#define PIF_COLOR_MAKE(r8, g8, b8) 		PIF_CONCAT2(PIF_COLOR_MAKE, PIF_COLOR_DEPTH)(r8, g8, b8)	///< Color of PIF_COLOR_DEPTH bits from 8 bit red, green and blue

/** @name Basic colors
 *  @{
 */
#define PIF_COLOR_BLACK   				((PifColor)PIF_COLOR_MAKE(0x00, 0x00, 0x00))	///< Black
#define PIF_COLOR_BLUE    				((PifColor)PIF_COLOR_MAKE(0x00, 0x00, 0xFF))	///< Blue
#define PIF_COLOR_GREEN   				((PifColor)PIF_COLOR_MAKE(0x00, 0xFF, 0x00))	///< Green
#define PIF_COLOR_RED     				((PifColor)PIF_COLOR_MAKE(0xFF, 0x00, 0x00))	///< Red
#define PIF_COLOR_GRAY   				((PifColor)PIF_COLOR_MAKE(0x7F, 0x7F, 0x7F))	///< Gray
#define PIF_COLOR_CYAN    				((PifColor)PIF_COLOR_MAKE(0x00, 0xFF, 0xFF))	///< Cyan
#define PIF_COLOR_MAGENTA 				((PifColor)PIF_COLOR_MAKE(0xFF, 0x00, 0xFF))	///< Magenta
#define PIF_COLOR_YELLOW  				((PifColor)PIF_COLOR_MAKE(0xFF, 0xFF, 0x00))	///< Yellow
#define PIF_COLOR_WHITE   				((PifColor)PIF_COLOR_MAKE(0xFF, 0xFF, 0xFF))	///< White
/** @} */

#define TFT_SETUP_DELAY_MS				0xFF	///< Command code in a setup table that means a delay instead of a command


/**
 * @brief 16 bit RGB 565 color.
 */
typedef uint16_t PifColor16;
/**
 * @brief 32 bit RGB 888 color.
 */
typedef uint32_t PifColor32;

/**
 * @brief Color of PIF_COLOR_DEPTH (16 or 32) bits.
 */
typedef PIF_CONCAT2(PifColor, PIF_COLOR_DEPTH) PifColor;

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
 * @brief Draws one pixel. Implemented by the controller driver.
 */
typedef void (*PifTftLcdDrawPixel)(PifTftLcd* p_parent, uint16_t x, uint16_t y, PifColor color);
/**
 * @brief Draws a horizontal line of len pixels. Implemented by the controller driver.
 */
typedef void (*PifTftLcdDrawHorLine)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);
/**
 * @brief Draws a vertical line of len pixels. Implemented by the controller driver.
 */
typedef void (*PifTftLcdDrawVerLine)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);
/**
 * @brief Fills a rectangle from (x1, y1) to (x2, y2). Implemented by the controller driver.
 */
typedef void (*PifTftLcdDrawFillRect)(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @class StPifTftLcd
 * @brief Base TFT LCD driver context shared by controller-specific implementations.
 */
struct StPifTftLcd
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;	///< Instance ID
    uint16_t _width;	///< Width in pixels at the current rotation
    uint16_t _height;	///< Height in pixels at the current rotation
    PifTftLcdRotation _rotation;	///< Current rotation

	// Private Member Variable

	// Read-only Function
    PifTftLcdSetRotation _fn_set_rotation;	///< Set by the controller driver
    PifTftLcdDrawPixel _fn_draw_pixel;	///< Set by the controller driver
    PifTftLcdDrawHorLine _fn_draw_hor_line;	///< Set by the controller driver
    PifTftLcdDrawVerLine _fn_draw_ver_line;	///< Set by the controller driver
    PifTftLcdDrawFillRect _fn_draw_fill_rect;	///< Set by the controller driver

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
 * @brief Clips an inclusive rectangle to the current panel size.
 * @param p_owner Pointer to the TFT LCD instance.
 * @param p_x1 Left X coordinate.
 * @param p_y1 Top Y coordinate.
 * @param p_x2 Right X coordinate, lowered to the last column if it lies beyond it.
 * @param p_y2 Bottom Y coordinate, lowered to the last row if it lies beyond it.
 * @return `FALSE` if nothing of the rectangle is on the panel or the corners are reversed, otherwise `TRUE`.
 */
BOOL pifTftLcd_ClipArea(PifTftLcd* p_owner, uint16_t* p_x1, uint16_t* p_y1, uint16_t* p_x2, uint16_t* p_y2);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TFT_LCD_H
