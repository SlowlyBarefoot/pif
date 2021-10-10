#include <string.h>

#include "pifPmlcdI2c.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


// commands
#define LCD_CLEAR_DISPLAY			0x01
#define LCD_RETURN_HOME				0x02
#define LCD_ENTRY_MODE_SET			0x04
#define LCD_DISPLAY_CONTROL			0x08
#define LCD_CURSOR_SHIFT			0x10
#define LCD_FUNCTION_SET			0x20
#define LCD_SET_CGRAM_ADDR			0x40
#define LCD_SET_DDRAM_ADDR			0x80

// flags for display entry mode
#define LCD_ENTRY_RIGHT				0x00
#define LCD_ENTRY_LEFT				0x02
#define LCD_ENTRY_SHIFT_INCREMENT	0x01
#define LCD_ENTRY_SHIFT_DECREMENT	0x00

// flags for display on/off control
#define LCD_DISPLAY_ON				0x04
#define LCD_DISPLAY_OFF				0x00
#define LCD_CURSOR_ON				0x02
#define LCD_CURSOR_OFF				0x00
#define LCD_BLINK_ON				0x01
#define LCD_BLINK_OFF				0x00

// flags for display/cursor shift
#define LCD_DISPLAY_MOVE			0x08
#define LCD_CURSOR_MOVE				0x00
#define LCD_MOVE_RIGHT				0x04
#define LCD_MOVE_LEFT				0x00

// flags for function set
#define LCD_8BIT_MODE				0x10
#define LCD_4BIT_MODE				0x00
#define LCD_2LINE					0x08
#define LCD_1LINE					0x00
#define LCD_5x10_DOTS				0x04
#define LCD_5x8_DOTS				0x00

// flags for backlight control
#define LCD_BACK_LIGHT				0x08
#define LCD_NO_BACK_LIGHT			0x00

#define En 0x04  // Enable bit
#define Rw 0x02  // Read/Write bit
#define Rs 0x01  // Register select bit


static BOOL _ExpanderWrite(PIF_stPmlcdI2c *pstOwner, uint8_t ucData)
{
	pstOwner->_stI2c.p_data[0] = ucData | pstOwner->__ucBacklightVal;
	return pifI2c_Write(&pstOwner->_stI2c, 1);
}

static void _PulseEnable(PIF_stPmlcdI2c *pstOwner, uint8_t ucData)
{
	_ExpanderWrite(pstOwner, ucData | En);	// En high
	pif_Delay1us(1);						// enable pulse must be >450ns

	_ExpanderWrite(pstOwner, ucData & ~En);	// En low
	pifTaskManager_YieldUs(50);					// commands need > 37us to settle
}

static void _Write4bits(PIF_stPmlcdI2c *pstOwner, uint8_t ucValue)
{
	_ExpanderWrite(pstOwner, ucValue);
	_PulseEnable(pstOwner, ucValue);
}

// write either command or data
static void _Send(PIF_stPmlcdI2c *pstOwner, uint8_t ucValue, uint8_t ucMode)
{
    _Write4bits(pstOwner, (ucValue & 0xf0) | ucMode);
	_Write4bits(pstOwner, ((ucValue << 4) & 0xf0) | ucMode);
}

/**
 * @fn pifPmlcdI2c_Create
 * @brief
 * @param usPifId
 * @param ucAddr
 * @return
 */
PIF_stPmlcdI2c *pifPmlcdI2c_Create(PifId usPifId, uint8_t ucAddr)
{
    PIF_stPmlcdI2c *pstOwner = NULL;

    if (!pif_act_timer1us) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

    pstOwner = calloc(sizeof(PIF_stPmlcdI2c), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (!pifI2c_Init(&pstOwner->_stI2c, usPifId, 2)) goto fail;
    pstOwner->_stI2c.addr = ucAddr;
    pstOwner->__ucBacklightVal = LCD_NO_BACK_LIGHT;
    pstOwner->__ucDisplayFunction = LCD_4BIT_MODE | LCD_1LINE | LCD_5x8_DOTS;
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
    return NULL;
}

/**
 * @fn pifPmlcdI2c_Destroy
 * @brief
 * @param ppstOwner
 */
void pifPmlcdI2c_Destroy(PIF_stPmlcdI2c **ppstOwner)
{
    if (*ppstOwner) {
    	if ((*ppstOwner)->_stI2c.p_data) {
        	free((*ppstOwner)->_stI2c.p_data);
            (*ppstOwner)->_stI2c.p_data = NULL;
    	}
    	free(*ppstOwner);
    	*ppstOwner = NULL;
    }
}

/**
 * @fn pifPmlcdI2c_Begin
 * @brief
 * @param pstOwner
 * @param ucLines
 * @param ucDotSize
 * @return
 */
void pifPmlcdI2c_Begin(PIF_stPmlcdI2c *pstOwner, uint8_t ucLines, uint8_t ucDotSize)
{
	if (ucLines > 1) {
		pstOwner->__ucDisplayFunction |= LCD_2LINE;
	}
	pstOwner->__ucNumLines = ucLines;

	// for some 1 line displays you can select a 10 pixel high font
	if ((ucDotSize != 0) && (ucLines == 1)) {
		pstOwner->__ucDisplayFunction |= LCD_5x10_DOTS;
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
	pifTaskManager_YieldMs(50);

	// Now we pull both RS and R/W low to begin commands
	_ExpanderWrite(pstOwner, pstOwner->__ucBacklightVal);	// reset expanderand turn backlight off (Bit 8 =1)
	pifTaskManager_YieldMs(1000);

  	//put the LCD into 4 bit mode
	// this is according to the hitachi HD44780 datasheet
	// figure 24, pg 46

	// we start in 8bit mode, try to set 4 bit mode
	_Write4bits(pstOwner, 0x03 << 4);
	pifTaskManager_YieldUs(4500); // wait min 4.1ms

	// second try
	_Write4bits(pstOwner, 0x03 << 4);
	pifTaskManager_YieldUs(4500); // wait min 4.1ms

	// third go!
	_Write4bits(pstOwner, 0x03 << 4);
	pifTaskManager_YieldUs(150);

	// finally, set to 4-bit interface
	_Write4bits(pstOwner, 0x02 << 4);

	// set # lines, font size, etc.
	_Send(pstOwner, LCD_FUNCTION_SET | pstOwner->__ucDisplayFunction, 0);

	// turn the display on with no cursor or blinking default
	pstOwner->__ucDisplayControl = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
	pifPmlcdI2c_Display(pstOwner);

	// clear it off
	pifPmlcdI2c_Clear(pstOwner);

	// Initialize to default text direction (for roman languages)
	pstOwner->__ucDisplayMode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DECREMENT;

	// set the entry mode
	_Send(pstOwner, LCD_ENTRY_MODE_SET | pstOwner->__ucDisplayMode, 0);

	pifPmlcdI2c_Home(pstOwner);
}

/**
 * @fn pifPmlcdI2c_Print
 * @brief
 * @param pstOwner
 * @param pcString
 */
void pifPmlcdI2c_Print(PIF_stPmlcdI2c *pstOwner, const char *pcString)
{
	uint8_t i;

	for (i = 0; i < strlen(pcString); i++) {
		_Send(pstOwner, pcString[i], Rs);
	}
}

/**
 * @fn pifPmlcdI2c_Printf
 * @brief
 * @param pstOwner
 * @param pcFormat
 */
void pifPmlcdI2c_Printf(PIF_stPmlcdI2c *pstOwner, const char *pcFormat, ...)
{
	va_list data;
	char cBuffer[32];

	va_start(data, pcFormat);
	pif_PrintFormat(cBuffer, &data, pcFormat);
	va_end(data);

	pifPmlcdI2c_Print(pstOwner, cBuffer);
}

/**
 * @fn pifPmlcdI2c_Clear
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_Clear(PIF_stPmlcdI2c *pstOwner)
{
	_Send(pstOwner, LCD_CLEAR_DISPLAY, 0);// clear display, set cursor position to zero
	pifTaskManager_YieldMs(2);  // this command takes a long time!
}

/**
 * @fn pifPmlcdI2c_Home
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_Home(PIF_stPmlcdI2c *pstOwner)
{
	_Send(pstOwner, LCD_RETURN_HOME, 0);  // set cursor position to zero
	pifTaskManager_YieldMs(2);  // this command takes a long time!
}

/**
 * @fn pifPmlcdI2c_SetCursor
 * @brief
 * @param pstOwner
 * @param ucCol
 * @param ucRow
 */
void pifPmlcdI2c_SetCursor(PIF_stPmlcdI2c *pstOwner, uint8_t ucCol, uint8_t ucRow)
{
	int nRowOffsets[] = { 0x00, 0x40, 0x14, 0x54 };

	if (ucRow > pstOwner->__ucNumLines) {
		ucRow = pstOwner->__ucNumLines - 1;    // we count rows starting w/0
	}
	_Send(pstOwner, LCD_SET_DDRAM_ADDR | (ucCol + nRowOffsets[ucRow]), 0);
}

/**
 * @fn pifPmlcdI2c_Display
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_Display(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayControl |= LCD_DISPLAY_ON;
	_Send(pstOwner, LCD_DISPLAY_CONTROL | pstOwner->__ucDisplayControl, 0);
}

/**
 * @fn pifPmlcdI2c_NoDisplay
 * @brief Turn the display on/off (quickly)
 * @param pstOwner
 */
void pifPmlcdI2c_NoDisplay(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayControl &= ~LCD_DISPLAY_ON;
	_Send(pstOwner, LCD_DISPLAY_CONTROL | pstOwner->__ucDisplayControl, 0);
}

/**
 * @fn pifPmlcdI2c_Cursor
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_Cursor(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayControl |= LCD_CURSOR_ON;
	_Send(pstOwner, LCD_DISPLAY_CONTROL | pstOwner->__ucDisplayControl, 0);
}

/**
 * @fn pifPmlcdI2c_NoCursor
 * @brief Turns the underline cursor on/off
 * @param pstOwner
 */
void pifPmlcdI2c_NoCursor(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayControl &= ~LCD_CURSOR_ON;
	_Send(pstOwner, LCD_DISPLAY_CONTROL | pstOwner->__ucDisplayControl, 0);
}

/**
 * @fn pifPmlcdI2c_Blink
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_Blink(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayControl |= LCD_BLINK_ON;
	_Send(pstOwner, LCD_DISPLAY_CONTROL | pstOwner->__ucDisplayControl, 0);
}

/**
 * @fn pifPmlcdI2c_NoBlink
 * @brief Turn on and off the blinking cursor
 * @param pstOwner
 */
void pifPmlcdI2c_NoBlink(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayControl &= ~LCD_BLINK_ON;
	_Send(pstOwner, LCD_DISPLAY_CONTROL | pstOwner->__ucDisplayControl, 0);
}

/**
 * @fn pifPmlcdI2c_ScrollDisplayLeft
 * @brief These commands scroll the display without changing the RAM
 * @param pstOwner
 */
void pifPmlcdI2c_ScrollDisplayLeft(PIF_stPmlcdI2c *pstOwner)
{
	_Send(pstOwner, LCD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_LEFT, 0);
}

/**
 * @fn pifPmlcdI2c_ScrollDisplayRight
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_ScrollDisplayRight(PIF_stPmlcdI2c *pstOwner)
{
	_Send(pstOwner, LCD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_RIGHT, 0);
}

/**
 * @fn pifPmlcdI2c_LeftToRight
 * @brief This is for text that flows Left to Right
 * @param pstOwner
 */
void pifPmlcdI2c_LeftToRight(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayMode |= LCD_ENTRY_LEFT;
	_Send(pstOwner, LCD_ENTRY_MODE_SET | pstOwner->__ucDisplayMode, 0);
}

/**
 * @fn pifPmlcdI2c_RightToLeft
 * @brief This is for text that flows Right to Left
 * @param pstOwner
 */
void pifPmlcdI2c_RightToLeft(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayMode &= ~LCD_ENTRY_LEFT;
	_Send(pstOwner, LCD_ENTRY_MODE_SET | pstOwner->__ucDisplayMode, 0);
}

/**
 * @fn pifPmlcdI2c_AutoScroll
 * @brief This will 'right justify' text from the cursor
 * @param pstOwner
 */
void pifPmlcdI2c_AutoScroll(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayMode |= LCD_ENTRY_SHIFT_INCREMENT;
	_Send(pstOwner, LCD_ENTRY_MODE_SET | pstOwner->__ucDisplayMode, 0);
}

/**
 * @fn pifPmlcdI2c_NoAutoScroll
 * @brief This will 'left justify' text from the cursor
 * @param pstOwner
 */
void pifPmlcdI2c_NoAutoScroll(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucDisplayMode &= ~LCD_ENTRY_SHIFT_INCREMENT;
	_Send(pstOwner, LCD_ENTRY_MODE_SET | pstOwner->__ucDisplayMode, 0);
}

/**
 * @fn pifPmlcdI2c_CreateChar
 * @brief Allows us to fill the first 8 CGRAM locations with custom characters
 * @param pstOwner
 * @param ucLocation
 * @param ucCharMap
 */
void pifPmlcdI2c_CreateChar(PIF_stPmlcdI2c *pstOwner, uint8_t ucLocation, uint8_t ucCharMap[])
{
	ucLocation &= 0x7; // we only have 8 locations 0-7
	_Send(pstOwner, LCD_SET_CGRAM_ADDR | (ucLocation << 3), 0);
	for (int i = 0; i < 8; i++) {
		_Send(pstOwner, ucCharMap[i], Rs);
	}
}

/**
 * @fn pifPmlcdI2c_Backlight
 * @brief
 * @param pstOwner
 */
void pifPmlcdI2c_Backlight(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucBacklightVal=LCD_BACK_LIGHT;
	_ExpanderWrite(pstOwner, 0);
}

/**
 * @fn pifPmlcdI2c_NoBacklight
 * @brief Turn the (optional) backlight off/on
 * @param pstOwner
 */
void pifPmlcdI2c_NoBacklight(PIF_stPmlcdI2c *pstOwner)
{
	pstOwner->__ucBacklightVal=LCD_NO_BACK_LIGHT;
	_ExpanderWrite(pstOwner, 0);
}
