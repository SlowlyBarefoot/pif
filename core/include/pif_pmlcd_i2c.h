#ifndef PIF_PMLCD_I2C_H
#define PIF_PMLCD_I2C_H


#include "pif_i2c.h"


#define PIF_PMLCD_DS_5x8			0x00
#define PIF_PMLCD_DS_5x10			0x04


/**
 * @class StPifPmlcdI2c
 * @brief
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
 * @brief
 * @param p_owner
 * @param id
 * @param p_port
 * @param addr
 * @return
 */
BOOL pifPmlcdI2c_Init(PifPmlcdI2c* p_owner, PifId id, PifI2cPort* p_port, uint8_t addr);

/**
 * @fn pifPmlcdI2c_Clear
 * @brief
 * @param p_owner
 */
void pifPmlcdI2c_Clear(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Begin
 * @brief
 * @param p_owner
 * @param lines
 * @param dot_size
 * @return
 */
BOOL pifPmlcdI2c_Begin(PifPmlcdI2c* p_owner, uint8_t lines, uint8_t dot_size);

/**
 * @fn pifPmlcdI2c_Print
 * @brief
 * @param p_owner
 * @param p_string
 * @return
 */
BOOL pifPmlcdI2c_Print(PifPmlcdI2c* p_owner, const char* p_string);

/**
 * @fn pifPmlcdI2c_Printf
 * @brief
 * @param p_owner
 * @param p_format
 * @return
 */
BOOL pifPmlcdI2c_Printf(PifPmlcdI2c* p_owner, const char* p_format, ...);

/**
 * @fn pifPmlcdI2c_DisplayClear
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_DisplayClear(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Home
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_Home(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_SetCursor
 * @brief
 * @param p_owner
 * @param col
 * @param row
 * @return
 */
BOOL pifPmlcdI2c_SetCursor(PifPmlcdI2c* p_owner, uint8_t col, uint8_t row);

/**
 * @fn pifPmlcdI2c_Display
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_Display(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoDisplay
 * @brief Turn the display on/off (quickly)
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_NoDisplay(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Cursor
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_Cursor(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoCursor
 * @brief Turns the underline cursor on/off
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_NoCursor(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_Blink
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_Blink(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoBlink
 * @brief Turn on and off the blinking cursor
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_NoBlink(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_ScrollDisplayLeft
 * @brief These commands scroll the display without changing the RAM
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_ScrollDisplayLeft(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_ScrollDisplayRight
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_ScrollDisplayRight(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_LeftToRight
 * @brief This is for text that flows Left to Right
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_LeftToRight(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_RightToLeft
 * @brief This is for text that flows Right to Left
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_RightToLeft(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_AutoScroll
 * @brief This will 'right justify' text from the cursor
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_AutoScroll(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoAutoScroll
 * @brief This will 'left justify' text from the cursor
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_NoAutoScroll(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_CreateChar
 * @brief Allows us to fill the first 8 CGRAM locations with custom characters
 * @param p_owner
 * @param location
 * @param char_map
 * @return
 */
BOOL pifPmlcdI2c_CreateChar(PifPmlcdI2c* p_owner, uint8_t location, uint8_t char_map[]);

/**
 * @fn pifPmlcdI2c_Backlight
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_Backlight(PifPmlcdI2c* p_owner);

/**
 * @fn pifPmlcdI2c_NoBacklight
 * @brief Turn the (optional) backlight off/on
 * @param p_owner
 * @return
 */
BOOL pifPmlcdI2c_NoBacklight(PifPmlcdI2c* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PMLCD_I2C_H
