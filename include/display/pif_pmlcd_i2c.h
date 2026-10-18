// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_pmlcd_i2c.h
 * @brief HD44780 compatible character LCD behind a PCF8574 I2C port expander.
 *
 * Text is written with the print functions, and the cursor, the display and the backlight are controlled through
 * the expander.
 */
#ifndef PIF_PMLCD_I2C_H
#define PIF_PMLCD_I2C_H


#include "communication/pif_i2c.h"


#define PIF_PMLCD_DS_5x8			0x00	///< 5 x 8 dot characters
#define PIF_PMLCD_DS_5x10			0x04	///< 5 x 10 dot characters

#ifndef PIF_PMLCD_I2C_PRINTF_SIZE
#define PIF_PMLCD_I2C_PRINTF_SIZE	32	///< Size of the buffer that pifPmlcdI2c_Printf() formats into
#endif


/**
 * @class StPifPmlcdI2c
 * @brief Driver context for an HD44780-compatible character LCD via an I2C expander.
 */
typedef struct StPifPmlcdI2c
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;	///< I2C device of the port expander

	// Private Member Variable
	uint8_t __display_function;
	uint8_t __display_control;
	uint8_t __display_mode;
	uint8_t __num_lines;
	uint8_t __backlight_val;
} PifPmlcdI2c;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the PM LCD I2C driver and registers the target I2C device.
 * @param p_owner Pointer to the driver instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param p_port I2C port used to communicate with the LCD expander.
 * @param addr 7-bit I2C slave address of the LCD expander.
 * @param p_client User-defined pointer passed to lower I2C layers.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Init(PifPmlcdI2c* p_owner, PifId id, PifI2cPort* p_port, uint8_t addr, void *p_client);

/**
 * @brief Releases driver resources and unregisters the I2C device.
 * @param p_owner Pointer to the driver instance to clear.
 */
void pifPmlcdI2c_Clear(PifPmlcdI2c* p_owner);

/**
 * @brief Executes LCD startup sequence and configures line count and font size.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param lines Number of display lines, 1 to 4.
 * @param dot_size Character font size (`PIF_PMLCD_DS_5x8` or `PIF_PMLCD_DS_5x10`). 5x10 only applies
 *        to a 1 line display.
 * @return `TRUE` if the LCD enters ready state, otherwise `FALSE`. Calls that need the line count,
 *         such as pifPmlcdI2c_SetCursor(), fail until this has succeeded.
 */
BOOL pifPmlcdI2c_Begin(PifPmlcdI2c* p_owner, uint8_t lines, uint8_t dot_size);

/**
 * @brief Writes a null-terminated string to the current cursor position.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param p_string Pointer to the text buffer to print.
 * @return `TRUE` if all characters are transmitted, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Print(PifPmlcdI2c* p_owner, const char* p_string);

/**
 * @brief Formats text with `printf`-style arguments and prints it to the LCD. The text is cut at
 *        `PIF_PMLCD_I2C_PRINTF_SIZE - 1` characters.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param p_format `printf`-style format string.
 * @return `TRUE` if the formatted string is transmitted successfully, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Printf(PifPmlcdI2c* p_owner, const char* p_format, ...);

/**
 * @brief Clears all characters on the display and resets cursor to home position.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_DisplayClear(PifPmlcdI2c* p_owner);

/**
 * @brief Moves cursor to the home position without clearing display memory.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Home(PifPmlcdI2c* p_owner);

/**
 * @brief Sets the text cursor to the requested column and row.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param col Zero-based target column.
 * @param row Zero-based target row. A row past the last line is taken as the last line.
 * @return `TRUE` if the cursor command is sent successfully, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_SetCursor(PifPmlcdI2c* p_owner, uint8_t col, uint8_t row);

/**
 * @brief Enables display output while preserving DDRAM/CGRAM contents.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the display control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Display(PifPmlcdI2c* p_owner);

/**
 * @brief Disables display output without deleting existing text data.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the display control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoDisplay(PifPmlcdI2c* p_owner);

/**
 * @brief Enables the underline cursor indicator.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the cursor control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Cursor(PifPmlcdI2c* p_owner);

/**
 * @brief Disables the underline cursor indicator.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the cursor control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoCursor(PifPmlcdI2c* p_owner);

/**
 * @brief Enables cursor blinking.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the blink control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Blink(PifPmlcdI2c* p_owner);

/**
 * @brief Disables cursor blinking.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the blink control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoBlink(PifPmlcdI2c* p_owner);

/**
 * @brief Scrolls the visible display window one position to the left.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the scroll command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_ScrollDisplayLeft(PifPmlcdI2c* p_owner);

/**
 * @brief Scrolls the visible display window one position to the right.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the scroll command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_ScrollDisplayRight(PifPmlcdI2c* p_owner);

/**
 * @brief Sets text entry direction to left-to-right.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_LeftToRight(PifPmlcdI2c* p_owner);

/**
 * @brief Sets text entry direction to right-to-left.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_RightToLeft(PifPmlcdI2c* p_owner);

/**
 * @brief Enables automatic display shift when new characters are written.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_AutoScroll(PifPmlcdI2c* p_owner);

/**
 * @brief Disables automatic display shift during character writes.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoAutoScroll(PifPmlcdI2c* p_owner);

/**
 * @brief Writes a custom character bitmap into one of the LCD CGRAM slots. The LCD is left
 *        addressing CGRAM, so call pifPmlcdI2c_SetCursor() or pifPmlcdI2c_Home() before printing.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param location CGRAM slot index (`0` to `7`).
 * @param char_map Pointer to an 8-byte character bitmap.
 * @return `TRUE` if all CGRAM bytes are written successfully, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_CreateChar(PifPmlcdI2c* p_owner, uint8_t location, const uint8_t char_map[]);

/**
 * @brief Turns on LCD backlight output (if connected through the expander).
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the expander write succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Backlight(PifPmlcdI2c* p_owner);

/**
 * @brief Turns off LCD backlight output (if connected through the expander).
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the expander write succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoBacklight(PifPmlcdI2c* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PMLCD_I2C_H
