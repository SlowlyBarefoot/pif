// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_tft_lcd.h
 * @brief Base of the LCD controllers that sit on an MCU bus and take commands and pixels on it.
 *
 * What such a controller has in common with the next one is here: the bus, the setup stream, the rotation, the
 * backlight and the drawing itself. A controller driver such as pif_ili9341 or pif_ssd1963 embeds a PifTftLcd as
 * its first member and fills in the two things that are its own, how a color becomes the words of a pixel and
 * how the address window is set, so that what is left in the driver is the commands of its controller and the
 * stream that sets it up.
 *
 * The application draws through the pifGraphics functions on &lcd.parent.parent, which pifIli9341_Graphics()
 * and pifSsd1963_Graphics() hand over, so the same code works with every controller and on the other graphic
 * displays of PIF.
 */
#ifndef PIF_TFT_LCD_H
#define PIF_TFT_LCD_H


#include "display/pif_graphics.h"


#define TFT_SETUP_DELAY_MS				0xFF	///< Command code in a setup table that means a delay instead of a command

#define PIF_TFT_LCD_MAX_COLOR_WORDS		3		///< Words the conversion of one pixel may take

/** 
 * @name Commands every controller of the DCS command set takes alike
 *  @{
 */
#define PIF_TFT_LCD_CMD_COL_ADDR		0x2A	///< set_column_address
#define PIF_TFT_LCD_CMD_PAGE_ADDR		0x2B	///< set_page_address
#define PIF_TFT_LCD_CMD_MEM_WRITE		0x2C	///< write_memory_start
/** @} */


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
typedef void (*PifActLcdReadCmd)(PifTftLcdCmd cmd, uint32_t *p_data, uint32_t size);

/**
 * @brief Writes a command with size words of parameters from p_data.
 */
typedef void (*PifActLcdWriteCmd)(PifTftLcdCmd cmd, uint32_t *p_data, uint32_t size);

/**
 * @brief Writes size words of pixel data.
 */
typedef void (*PifActLcdWriteData)(uint32_t *p_data, uint32_t size);

/**
 * @brief Writes the size words of one pixel len times, to fill an area.
 */
typedef void (*PifActLcdWriteRepeat)(uint32_t *p_data, uint8_t size, uint32_t len);

/**
 * @brief Sets the brightness of the backlight.
 */
typedef void (*PifActLcdBackLight)(uint8_t level);

/**
 * @brief Changes the rotation. Implemented by the controller driver.
 */
typedef BOOL (*PifTftLcdSetRotation)(PifTftLcd *p_parent, PifTftLcdRotation rotation);

/**
 * @brief Turns a color into the words the interface sends for one pixel, and returns their number, at most
 *        PIF_TFT_LCD_MAX_COLOR_WORDS. Implemented by the controller driver.
 */
typedef uint8_t (*PifTftLcdConvertColor)(PifTftLcd *p_owner, PifColor color, uint32_t *p_data);

/**
 * @brief Sets the address window to (x1, y1) to (x2, y2) and leaves the controller taking pixels. Implemented
 *        by the controller driver, which may take pifTftLcd_SetAddressDcs() as it is. Called with the chip
 *        already selected.
 */
typedef void (*PifTftLcdSetAddress)(PifTftLcd *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief Shown each entry of a setup stream by pifTftLcd_ScanSetup(), so that a driver may read out of the
 *        stream what it has to know before it runs.
 */
typedef void (*PifTftLcdSetupPeek)(PifTftLcd *p_owner, uint8_t cmd, const uint8_t *p_param, uint8_t len);

/**
 * @class StPifTftLcd
 * @brief Base driver context shared by the controller-specific implementations.
 */
struct StPifTftLcd
{
	/// Drawing surface of the panel. The parent variable must be at the beginning of this structure.
	PifGraphics parent;

	// Public Member Variable

	// Read-only Member Variable
    PifTftLcdRotation _rotation;	///< Current rotation

	// Private Member Variable
	/// How far a command code and its parameters sit up the data bus, which an interface that carries them in
	/// the high bits needs. 0 on an interface that puts them in the low bits.
	uint8_t __command_shift;

	// Read-only Function
    PifTftLcdSetRotation _fn_set_rotation;		///< Set by the controller driver. May be NULL.
    PifTftLcdConvertColor _fn_convert_color;	///< Set by the controller driver
    PifTftLcdSetAddress _fn_set_address;		///< Set by the controller driver

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
 * @brief Initializes the common context with geometry and initial rotation, and fills in the drawing functions
 *        of its surface. A controller driver calls it from its own init and then sets _fn_convert_color,
 *        _fn_set_address and, if it turns, _fn_set_rotation.
 * @param p_owner Pointer to the TFT LCD instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param width Native panel width in pixels.
 * @param height Native panel height in pixels.
 * @param rotation Rotation the panel is in. Only recorded, with width and height swapped for 90 and
 *        270 degrees; the controller itself is set by the setup stream or the set-rotation function.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTftLcd_Init(PifTftLcd *p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation);

/**
 * @brief Hands over the drawing surface of the panel, which the pifGraphics functions draw on.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @return The PifGraphics of the panel.
 */
PIF_STATIC_INLINE PifGraphics *pifTftLcd_Graphics(PifTftLcd *p_owner) { return &p_owner->parent; }

/**
 * @brief Tells the driver how far a command code and its parameters sit up the data bus. A controller driver
 *        calls it when its interface is chosen.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param shift Bits to move a command code and each parameter up by.
 */
void pifTftLcd_SetCommandShift(PifTftLcd *p_owner, uint8_t shift);

/**
 * @brief Turns the picture, which the driver of the controller carries out.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param rotation Rotation to turn to.
 * @return `TRUE` if the panel turned, otherwise `FALSE` with pif_error set to E_CANNOT_USE when the driver
 *         does not turn at all.
 */
BOOL pifTftLcd_SetRotation(PifTftLcd *p_owner, PifTftLcdRotation rotation);

/**
 * @brief Sets the brightness of the backlight, if the board drives it.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param level Brightness, as the board of the application reads it.
 * @return `TRUE` if the backlight was set, otherwise `FALSE` when the board has no act_backlight.
 */
BOOL pifTftLcd_SetBackLight(PifTftLcd *p_owner, uint8_t level);

/**
 * @brief Writes one command with its parameters, each moved up the bus as pifTftLcd_SetCommandShift() said.
 *        Called with the chip already selected.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param cmd Command code, as the datasheet of the controller gives it.
 * @param p_param Parameters, one byte each, or `NULL` for none.
 * @param len Number of parameters, at most 16.
 */
void pifTftLcd_WriteCmd(PifTftLcd *p_owner, PifTftLcdCmd cmd, const uint8_t *p_param, uint8_t len);

/**
 * @brief Writes a command and reads its answer, moved back down the bus. Called with the chip already selected.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param cmd Command code, as the datasheet of the controller gives it.
 * @param p_data Receives the answer, one byte a word, the first of which is the dummy the controller sends.
 * @param len Words to read.
 * @return `TRUE` if the answer was read, otherwise `FALSE` when the board cannot read the bus.
 */
BOOL pifTftLcd_ReadCmd(PifTftLcd *p_owner, PifTftLcdCmd cmd, uint32_t *p_data, uint8_t len);

/**
 * @brief Walks a setup stream without sending anything, so that a driver may read out of it what it has to
 *        know, and makes sure every entry fits.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param p_setup Setup stream terminated by `0`: a command code, the number of its parameters, the parameters.
 *        A command of `TFT_SETUP_DELAY_MS` means a wait of that many milliseconds instead.
 * @param peek Shown each entry that is not a delay. May be `NULL`.
 * @return `TRUE` if every entry holds at most 16 parameters, otherwise `FALSE` with pif_error set to
 *         E_WRONG_DATA.
 */
BOOL pifTftLcd_ScanSetup(PifTftLcd *p_owner, const uint8_t *p_setup, PifTftLcdSetupPeek peek);

/**
 * @brief Sends a setup stream, waiting where it asks for a wait. Called with the chip already selected.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param p_setup Setup stream, as pifTftLcd_ScanSetup() describes it and has looked over.
 */
void pifTftLcd_RunSetup(PifTftLcd *p_owner, const uint8_t *p_setup);

/**
 * @brief Sets the address window with the commands of the DCS command set, which the controllers take alike.
 *        A driver whose controller does so puts this in its _fn_set_address.
 * @param p_owner Pointer to an initialized TFT LCD instance.
 * @param x1 Left X coordinate.
 * @param y1 Top Y coordinate.
 * @param x2 Right X coordinate.
 * @param y2 Bottom Y coordinate.
 */
void pifTftLcd_SetAddressDcs(PifTftLcd *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief Draws a single pixel. pifTftLcd_Init() puts it in the surface of the panel.
 * @param p_parent Pointer to the drawing surface of the panel.
 * @param x Horizontal pixel coordinate.
 * @param y Vertical pixel coordinate.
 * @param color Pixel color.
 */
void pifTftLcd_DrawPixel(PifGraphics *p_parent, uint16_t x, uint16_t y, PifColor color);

/**
 * @brief Draws a horizontal line in one address window. pifTftLcd_Init() puts it in the surface of the panel.
 * @param p_parent Pointer to the drawing surface of the panel.
 * @param x Start X coordinate.
 * @param y Fixed Y coordinate.
 * @param len Number of pixels to draw; the part beyond the panel is not drawn.
 * @param color Line color.
 */
void pifTftLcd_DrawHorLine(PifGraphics *p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @brief Draws a vertical line in one address window. pifTftLcd_Init() puts it in the surface of the panel.
 * @param p_parent Pointer to the drawing surface of the panel.
 * @param x Fixed X coordinate.
 * @param y Start Y coordinate.
 * @param len Number of pixels to draw; the part beyond the panel is not drawn.
 * @param color Line color.
 */
void pifTftLcd_DrawVerLine(PifGraphics *p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @brief Fills a rectangle in one address window. pifTftLcd_Init() puts it in the surface of the panel.
 * @param p_parent Pointer to the drawing surface of the panel.
 * @param x1 Left X coordinate of the rectangle.
 * @param y1 Top Y coordinate of the rectangle.
 * @param x2 Right X coordinate of the rectangle.
 * @param y2 Bottom Y coordinate of the rectangle.
 * @param color Fill color.
 */
void pifTftLcd_DrawFillRect(PifGraphics *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @brief Draws a rectangular area from a color map, which is how a graphics library hands over what it drew.
 *        Nothing is drawn if the area does not lie entirely on the panel.
 * @param p_parent Pointer to the drawing surface of the panel.
 * @param x1 Left X coordinate of the destination area.
 * @param y1 Top Y coordinate of the destination area.
 * @param x2 Right X coordinate of the destination area.
 * @param y2 Bottom Y coordinate of the destination area.
 * @param p_color_map Pointer to source pixel data in row-major order.
 */
void pifTftLcd_DrawArea(PifGraphics *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor *p_color_map);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TFT_LCD_H
