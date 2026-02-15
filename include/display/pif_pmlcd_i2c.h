#ifndef PIF_PMLCD_I2C_H
#define PIF_PMLCD_I2C_H


#include "communication/pif_i2c.h"


#define PIF_PMLCD_DS_5x8			0x00
#define PIF_PMLCD_DS_5x10			0x04


/**
 * @class StPifPmlcdI2c
 * @brief Driver context for an HD44780-compatible character LCD via an I2C expander.
 */
typedef struct StPifPmlcdI2c
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;

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
 * @fn pifPmlcdI2c_Init
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
 * @fn pifPmlcdI2c_Clear
 * @brief Releases driver resources and unregisters the I2C device.
 * @param p_owner Pointer to the driver instance to clear.
 */
void pifPmlcdI2c_Clear(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Begin
 * @brief Executes LCD startup sequence and configures line count and font size.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param lines Number of display lines (typically 1, 2, or 4 depending on module).
 * @param dot_size Character font size (`PIF_PMLCD_DS_5x8` or `PIF_PMLCD_DS_5x10`).
 * @return `TRUE` if the LCD enters ready state, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Begin(PifPmlcdI2c* p_owner, uint8_t lines, uint8_t dot_size);

/**
 * @fn pifPmlcdI2c_Print
 * @brief Writes a null-terminated string to the current cursor position.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param p_string Pointer to the text buffer to print.
 * @return `TRUE` if all characters are transmitted, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Print(PifPmlcdI2c* p_owner, const char* p_string);

/**
 * @fn pifPmlcdI2c_Printf
 * @brief Formats text with `printf`-style arguments and prints it to the LCD.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param p_format `printf`-style format string.
 * @return `TRUE` if the formatted string is transmitted successfully, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Printf(PifPmlcdI2c* p_owner, const char* p_format, ...);

/**
 * @fn pifPmlcdI2c_DisplayClear
 * @brief Clears all characters on the display and resets cursor to home position.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_DisplayClear(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Home
 * @brief Moves cursor to the home position without clearing display memory.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` on successful command transfer, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Home(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_SetCursor
 * @brief Sets the text cursor to the requested column and row.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param col Zero-based target column.
 * @param row Zero-based target row.
 * @return `TRUE` if the cursor command is sent successfully, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_SetCursor(PifPmlcdI2c* p_owner, uint8_t col, uint8_t row);

/**
 * @fn pifPmlcdI2c_Display
 * @brief Enables display output while preserving DDRAM/CGRAM contents.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the display control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Display(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoDisplay
 * @brief Disables display output without deleting existing text data.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the display control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoDisplay(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Cursor
 * @brief Enables the underline cursor indicator.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the cursor control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Cursor(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoCursor
 * @brief Disables the underline cursor indicator.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the cursor control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoCursor(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Blink
 * @brief Enables cursor blinking.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the blink control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Blink(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoBlink
 * @brief Disables cursor blinking.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the blink control command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoBlink(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_ScrollDisplayLeft
 * @brief Scrolls the visible display window one position to the left.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the scroll command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_ScrollDisplayLeft(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_ScrollDisplayRight
 * @brief Scrolls the visible display window one position to the right.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the scroll command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_ScrollDisplayRight(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_LeftToRight
 * @brief Sets text entry direction to left-to-right.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_LeftToRight(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_RightToLeft
 * @brief Sets text entry direction to right-to-left.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_RightToLeft(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_AutoScroll
 * @brief Enables automatic display shift when new characters are written.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_AutoScroll(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoAutoScroll
 * @brief Disables automatic display shift during character writes.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the entry mode command succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoAutoScroll(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_CreateChar
 * @brief Writes a custom character bitmap into one of the LCD CGRAM slots.
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @param location CGRAM slot index (`0` to `7`).
 * @param char_map Pointer to an 8-byte character bitmap.
 * @return `TRUE` if all CGRAM bytes are written successfully, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_CreateChar(PifPmlcdI2c* p_owner, uint8_t location, uint8_t char_map[]);

/**
 * @fn pifPmlcdI2c_Backlight
 * @brief Turns on LCD backlight output (if connected through the expander).
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the expander write succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_Backlight(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoBacklight
 * @brief Turns off LCD backlight output (if connected through the expander).
 * @param p_owner Pointer to an initialized PM LCD I2C driver instance.
 * @return `TRUE` if the expander write succeeds, otherwise `FALSE`.
 */
BOOL pifPmlcdI2c_NoBacklight(PifPmlcdI2c* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PMLCD_I2C_H
