// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ssd1306.h
 * @brief Driver of the SSD1306 monochrome OLED controller on I2C, as the GM009605 module carries it
 *        (0.96 inch, 128 x 64).
 *
 * The controller has no readable frame memory on I2C, so the driver keeps a frame buffer of its own. The
 * pifGraphics functions on pifSsd1306_Graphics() change that buffer alone and pifSsd1306_Update() sends what
 * changed to the panel, page by page. Nothing appears before an update.
 *
 * A GM009605 is wired as a 4 pin I2C module: GND, VCC (3.3 V, 5 V tolerant on most boards), SCK, which is the
 * I2C clock, and SDA. Its address is PIF_SSD1306_I2C_ADDR unless the board ties SA0 high.
 */
#ifndef PIF_SSD1306_H
#define PIF_SSD1306_H


#include "communication/pif_i2c.h"
#include "display/pif_graphics.h"


#define PIF_SSD1306_I2C_ADDR		0x3C	///< 7 bit slave address with SA0 low, which the GM009605 has
#define PIF_SSD1306_I2C_ADDR_ALT	0x3D	///< 7 bit slave address with SA0 high

#define SSD1306_WIDTH				128	///< Columns of the GM009605, and the most the controller drives
#define SSD1306_HEIGHT				64	///< Rows of the GM009605, and the most the controller drives
#define SSD1306_MAX_PAGES			(SSD1306_HEIGHT / 8)	///< Rows of 8 the controller drives

/** @name Colors of the panel
 *
 *  The panel has one bit per pixel, so a color it is drawn with is one of these. `SSD1306_COLOR_BLACK` and
 *  `SSD1306_COLOR_WHITE` are the same values as `PIF_COLOR_BLACK` and a color that is not black, so drawing code
 *  written for a color display works here as it stands.
 *  @{
 */
#define SSD1306_COLOR_BLACK			((PifColor)0)	///< Turns the pixels off
#define SSD1306_COLOR_WHITE			((PifColor)1)	///< Turns the pixels on, as every color but black and `SSD1306_COLOR_INVERSE` does
#define SSD1306_COLOR_INVERSE		((PifColor)2)	///< Turns each pixel the other way
/** @} */

#ifndef PIF_SSD1306_BEGIN_DELAY_MS
/// How long pifSsd1306_Begin() waits at its end for the charge pump to reach its voltage, as the application
/// note of the controller asks. 0 for a platform that waits for the panel somewhere else.
#define PIF_SSD1306_BEGIN_DELAY_MS	100
#endif


/**
 * @brief Commands of the SSD1306, as named in its datasheet.
 *
 * The ones marked with a range take their argument in the low bits of the command byte itself.
 */
typedef enum EnPifSsd1306Cmd
{
	SSD1306_CMD_SET_LOW_COL			= 0x00,	///< 00h~0Fh, low nibble of the column in page addressing
	SSD1306_CMD_SET_HIGH_COL		= 0x10,	///< 10h~1Fh, high nibble of the column in page addressing
	SSD1306_CMD_SET_MEMORY_MODE		= 0x20,	///< Takes a PifSsd1306MemoryMode
	SSD1306_CMD_SET_COL_ADDR		= 0x21,	///< Takes the first and the last column
	SSD1306_CMD_SET_PAGE_ADDR		= 0x22,	///< Takes the first and the last page

	SSD1306_CMD_SET_SCROLL_RIGHT	= 0x26,
	SSD1306_CMD_SET_SCROLL_LEFT		= 0x27,
	SSD1306_CMD_SET_SCROLL_VER_RIGHT = 0x29,
	SSD1306_CMD_SET_SCROLL_VER_LEFT	= 0x2A,
	SSD1306_CMD_DEACTIVATE_SCROLL	= 0x2E,
	SSD1306_CMD_ACTIVATE_SCROLL		= 0x2F,

	SSD1306_CMD_SET_START_LINE		= 0x40,	///< 40h~7Fh, first row of the frame memory shown

	SSD1306_CMD_SET_CONTRAST		= 0x81,	///< Takes 0 to 255
	SSD1306_CMD_SET_CHARGE_PUMP		= 0x8D,	///< Takes SSD1306_CHARGE_PUMP_OFF or SSD1306_CHARGE_PUMP_ON

	SSD1306_CMD_SET_SEG_REMAP_OFF	= 0xA0,	///< Column 0 is SEG0
	SSD1306_CMD_SET_SEG_REMAP_ON	= 0xA1,	///< Column 0 is SEG127, which a GM009605 needs
	SSD1306_CMD_SET_VER_SCROLL_AREA	= 0xA3,
	SSD1306_CMD_ENTIRE_DISP_OFF		= 0xA4,	///< Shows the frame memory again
	SSD1306_CMD_ENTIRE_DISP_ON		= 0xA5,	///< Lights every pixel, whatever the frame memory holds
	SSD1306_CMD_SET_NORMAL_DISP		= 0xA6,
	SSD1306_CMD_SET_INVERSE_DISP	= 0xA7,
	SSD1306_CMD_SET_MULTIPLEX		= 0xA8,	///< Takes the row count less one
	SSD1306_CMD_SET_DISP_OFF		= 0xAE,
	SSD1306_CMD_SET_DISP_ON			= 0xAF,

	SSD1306_CMD_SET_PAGE_START		= 0xB0,	///< B0h~B7h, page in page addressing

	SSD1306_CMD_SET_COM_SCAN_INC	= 0xC0,	///< Scans COM0 to COM[n-1]
	SSD1306_CMD_SET_COM_SCAN_DEC	= 0xC8,	///< Scans COM[n-1] to COM0, which a GM009605 needs

	SSD1306_CMD_SET_DISP_OFFSET		= 0xD3,	///< Takes the first COM line of the display
	SSD1306_CMD_SET_DISP_CLOCK		= 0xD5,	///< Takes the oscillator frequency and the clock divider
	SSD1306_CMD_SET_PRECHARGE		= 0xD9,	///< Takes phase 1 and phase 2 of the pre-charge period
	SSD1306_CMD_SET_COM_PINS		= 0xDA,	///< Takes the COM pin layout of the panel
	SSD1306_CMD_SET_VCOMH_DESELECT	= 0xDB,	///< Takes the VCOMH deselect level

	SSD1306_CMD_NOP					= 0xE3
} PifSsd1306Cmd;

/**
 * @brief Argument of SSD1306_CMD_SET_MEMORY_MODE: how the data pointer moves while pixels come in.
 */
typedef enum EnPifSsd1306MemoryMode
{
	SSD1306_MM_HORIZONTAL	= 0x00,	///< Along the page, then on to the next page. Used by the driver.
	SSD1306_MM_VERTICAL		= 0x01,	///< Down the column, then on to the next column
	SSD1306_MM_PAGE			= 0x02	///< Along the page and no further
} PifSsd1306MemoryMode;

#define SSD1306_CHARGE_PUMP_OFF		0x10	///< Argument of SSD1306_CMD_SET_CHARGE_PUMP for an external VCC
#define SSD1306_CHARGE_PUMP_ON		0x14	///< Argument of SSD1306_CMD_SET_CHARGE_PUMP for the internal one, which a GM009605 has


/**
 * @class StPifSsd1306
 * @brief Driver context of an SSD1306 panel on I2C, with its frame buffer.
 */
typedef struct StPifSsd1306
{
	/// Drawing surface of the panel. The parent variable must be at the beginning of this structure.
	PifGraphics parent;

	// Public Member Variable

	// Read-only Member Variable
	PifDeviceReg *_p_device;	///< I2C device of the controller
	uint8_t _pages;	///< Rows of 8 of the panel, the height divided by 8

	// Private Member Variable
	uint8_t *__p_buffer;	///< width * _pages bytes, a page after another
	uint8_t __dirty_first[SSD1306_MAX_PAGES];	///< First column of a page that the panel has not got yet
	uint8_t __dirty_last[SSD1306_MAX_PAGES];	///< Last such column. Past __dirty_first the page is clean.

	struct {
		uint8_t begin		: 1;	///< pifSsd1306_Begin() has run
		uint8_t display		: 1;
		uint8_t inverse		: 1;
		uint8_t flip_hor	: 1;
		uint8_t flip_ver	: 1;
	} __bt;
} PifSsd1306;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the driver, registers the panel as an I2C device and allocates the frame buffer. The panel
 *        itself is only set up by pifSsd1306_Begin(), so this may run before the bus is up.
 * @param p_owner Pointer to the driver instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param p_port I2C port the panel is on.
 * @param addr 7-bit I2C slave address of the panel, `PIF_SSD1306_I2C_ADDR` for a GM009605.
 * @param p_client User-defined pointer passed to lower I2C layers.
 * @param width Columns of the panel, 1 to `SSD1306_WIDTH`. A GM009605 has `SSD1306_WIDTH`.
 * @param height Rows of the panel: 16, 32 or `SSD1306_HEIGHT`. A GM009605 has `SSD1306_HEIGHT`.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE` with pif_error set.
 */
BOOL pifSsd1306_Init(PifSsd1306 *p_owner, PifId id, PifI2cPort *p_port, uint8_t addr, void *p_client,
		uint8_t width, uint8_t height);

/**
 * @brief Releases the frame buffer and unregisters the I2C device.
 * @param p_owner Pointer to the driver instance to clear.
 */
void pifSsd1306_Clear(PifSsd1306 *p_owner);

/**
 * @brief Hands over the drawing surface of the panel, which the pifGraphics functions draw on. It is there as
 *        soon as pifSsd1306_Init() has run, so an application may draw before the panel is set up.
 * @param p_owner Pointer to an initialized driver instance.
 * @return The PifGraphics of the panel.
 */
PIF_STATIC_INLINE PifGraphics* pifSsd1306_Graphics(PifSsd1306 *p_owner) { return &p_owner->parent; }

/**
 * @brief Runs the startup sequence of the panel, clears its frame memory and turns the display on. The frame
 *        buffer is cleared with it, so what was drawn before is gone.
 *
 * It may run again later to set the panel up anew. The contrast, the inverse mode and the flip go back to what
 * they are after a reset.
 * @param p_owner Pointer to an initialized driver instance.
 * @return `TRUE` if the panel is set up, otherwise `FALSE`. The functions that talk to the panel fail until
 *         this has succeeded.
 */
BOOL pifSsd1306_Begin(PifSsd1306 *p_owner);

/**
 * @brief Sends the part of the frame buffer that the panel has not got yet. Pages the draw functions did not
 *        touch are not sent.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @return `TRUE` if everything was sent, otherwise `FALSE`. What a failed call did not send stays marked, so
 *         another call sends it again.
 */
BOOL pifSsd1306_Update(PifSsd1306 *p_owner);

/**
 * @brief Sends the whole frame buffer, whether it changed or not.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @return `TRUE` if everything was sent, otherwise `FALSE`.
 */
BOOL pifSsd1306_UpdateAll(PifSsd1306 *p_owner);

/**
 * @brief Tells whether a pixel of the frame buffer is on.
 * @param p_owner Pointer to an initialized driver instance.
 * @param x Column, 0 at the left.
 * @param y Row, 0 at the top.
 * @return `TRUE` if the pixel is on, otherwise `FALSE`. A pixel off the panel is `FALSE`.
 */
BOOL pifSsd1306_GetPixel(PifSsd1306 *p_owner, uint16_t x, uint16_t y);

/**
 * @brief Turns the display on, with the frame memory kept.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifSsd1306_Display(PifSsd1306 *p_owner);

/**
 * @brief Turns the display off, with the frame memory kept. The panel draws almost nothing then.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifSsd1306_NoDisplay(PifSsd1306 *p_owner);

/**
 * @brief Sets the brightness of the panel.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @param contrast 0 to 255. pifSsd1306_Begin() leaves it at 207.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifSsd1306_SetContrast(PifSsd1306 *p_owner, uint8_t contrast);

/**
 * @brief Lets the panel show every pixel the other way round, without touching the frame memory.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifSsd1306_Inverse(PifSsd1306 *p_owner);

/**
 * @brief Lets the panel show the frame memory as it is again.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifSsd1306_NoInverse(PifSsd1306 *p_owner);

/**
 * @brief Turns the picture over, which is how the panel is mounted the other way round. The frame buffer is not
 *        touched, so what is on the panel turns over at once, with no update.
 * @param p_owner Pointer to a driver instance that pifSsd1306_Begin() has set up.
 * @param flip_hor `TRUE` mirrors the columns.
 * @param flip_ver `TRUE` mirrors the rows.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`. Both `FALSE` is how a GM009605 is mounted,
 *         and what pifSsd1306_Begin() leaves behind.
 */
BOOL pifSsd1306_SetFlip(PifSsd1306 *p_owner, BOOL flip_hor, BOOL flip_ver);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SSD1306_H
