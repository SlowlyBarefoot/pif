#include <string.h>

#include "pif_pmlcd_i2c.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
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


static BOOL _expanderWrite(PifPmlcdI2c* p_owner, uint8_t data)
{
	p_owner->_i2c.p_data[0] = data | p_owner->__backlight_val;
	return pifI2c_Write(&p_owner->_i2c, 1);
}

static void _pulseEnable(PifPmlcdI2c* p_owner, uint8_t data)
{
	_expanderWrite(p_owner, data | En);	// En high
	pif_Delay1us(1);						// enable pulse must be >450ns

	_expanderWrite(p_owner, data & ~En);	// En low
	pifTaskManager_YieldUs(50);					// commands need > 37us to settle
}

static void _write4bits(PifPmlcdI2c* p_owner, uint8_t value)
{
	_expanderWrite(p_owner, value);
	_pulseEnable(p_owner, value);
}

// write either command or data
static void _send(PifPmlcdI2c* p_owner, uint8_t value, uint8_t mode)
{
    _write4bits(p_owner, (value & 0xf0) | mode);
	_write4bits(p_owner, ((value << 4) & 0xf0) | mode);
}

PifPmlcdI2c* pifPmlcdI2c_Create(PifId id, uint8_t addr)
{
    PifPmlcdI2c* p_owner = malloc(sizeof(PifPmlcdI2c));
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

    if (!pifPmlcdI2c_Init(p_owner, id, addr)) {
		pifPmlcdI2c_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

void pifPmlcdI2c_Destroy(PifPmlcdI2c** pp_owner)
{
    if (*pp_owner) {
		pifPmlcdI2c_Clear(*pp_owner);
    	free(*pp_owner);
    	*pp_owner = NULL;
    }
}

BOOL pifPmlcdI2c_Init(PifPmlcdI2c* p_owner, PifId id, uint8_t addr)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    if (!pif_act_timer1us) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifPmlcdI2c));

    if (!pifI2c_Init(&p_owner->_i2c, id, 2)) goto fail;
    p_owner->_i2c.addr = addr;
    p_owner->__backlight_val = LCD_NO_BACK_LIGHT;
    p_owner->__display_function = LCD_4BIT_MODE | LCD_1LINE | LCD_5x8_DOTS;
    return TRUE;

fail:
	pifPmlcdI2c_Clear(p_owner);
    return FALSE;
}

void pifPmlcdI2c_Clear(PifPmlcdI2c* p_owner)
{
	if (p_owner->_i2c.p_data) {
		free(p_owner->_i2c.p_data);
		p_owner->_i2c.p_data = NULL;
	}
}

void pifPmlcdI2c_Begin(PifPmlcdI2c* p_owner, uint8_t lines, uint8_t dot_size)
{
	if (lines > 1) {
		p_owner->__display_function |= LCD_2LINE;
	}
	p_owner->__num_lines = lines;

	// for some 1 line displays you can select a 10 pixel high font
	if ((dot_size != 0) && (lines == 1)) {
		p_owner->__display_function |= LCD_5x10_DOTS;
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
	pifTaskManager_YieldMs(50);

	// Now we pull both RS and R/W low to begin commands
	_expanderWrite(p_owner, p_owner->__backlight_val);	// reset expanderand turn backlight off (Bit 8 =1)
	pifTaskManager_YieldMs(1000);

  	//put the LCD into 4 bit mode
	// this is according to the hitachi HD44780 datasheet
	// figure 24, pg 46

	// we start in 8bit mode, try to set 4 bit mode
	_write4bits(p_owner, 0x03 << 4);
	pifTaskManager_YieldUs(4500); // wait min 4.1ms

	// second try
	_write4bits(p_owner, 0x03 << 4);
	pifTaskManager_YieldUs(4500); // wait min 4.1ms

	// third go!
	_write4bits(p_owner, 0x03 << 4);
	pifTaskManager_YieldUs(150);

	// finally, set to 4-bit interface
	_write4bits(p_owner, 0x02 << 4);

	// set # lines, font size, etc.
	_send(p_owner, LCD_FUNCTION_SET | p_owner->__display_function, 0);

	// turn the display on with no cursor or blinking default
	p_owner->__display_control = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
	pifPmlcdI2c_Display(p_owner);

	// clear it off
	pifPmlcdI2c_Clear(p_owner);

	// Initialize to default text direction (for roman languages)
	p_owner->__display_mode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DECREMENT;

	// set the entry mode
	_send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);

	pifPmlcdI2c_Home(p_owner);
}

void pifPmlcdI2c_Print(PifPmlcdI2c* p_owner, const char* p_string)
{
	uint8_t i;

	for (i = 0; i < strlen(p_string); i++) {
		_send(p_owner, p_string[i], Rs);
	}
}

void pifPmlcdI2c_Printf(PifPmlcdI2c* p_owner, const char* p_format, ...)
{
	va_list data;
	char buffer[32];

	va_start(data, p_format);
	pif_PrintFormat(buffer, &data, p_format);
	va_end(data);

	pifPmlcdI2c_Print(p_owner, buffer);
}

void pifPmlcdI2c_DisplayClear(PifPmlcdI2c* p_owner)
{
	_send(p_owner, LCD_CLEAR_DISPLAY, 0);// clear display, set cursor position to zero
	pifTaskManager_YieldMs(2);  // this command takes a long time!
}

void pifPmlcdI2c_Home(PifPmlcdI2c* p_owner)
{
	_send(p_owner, LCD_RETURN_HOME, 0);  // set cursor position to zero
	pifTaskManager_YieldMs(2);  // this command takes a long time!
}

void pifPmlcdI2c_SetCursor(PifPmlcdI2c* p_owner, uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };

	if (row > p_owner->__num_lines) {
		row = p_owner->__num_lines - 1;    // we count rows starting w/0
	}
	_send(p_owner, LCD_SET_DDRAM_ADDR | (col + row_offsets[row]), 0);
}

void pifPmlcdI2c_Display(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control |= LCD_DISPLAY_ON;
	_send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

void pifPmlcdI2c_NoDisplay(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control &= ~LCD_DISPLAY_ON;
	_send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

void pifPmlcdI2c_Cursor(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control |= LCD_CURSOR_ON;
	_send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

void pifPmlcdI2c_NoCursor(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control &= ~LCD_CURSOR_ON;
	_send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

void pifPmlcdI2c_Blink(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control |= LCD_BLINK_ON;
	_send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

void pifPmlcdI2c_NoBlink(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control &= ~LCD_BLINK_ON;
	_send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

void pifPmlcdI2c_ScrollDisplayLeft(PifPmlcdI2c* p_owner)
{
	_send(p_owner, LCD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_LEFT, 0);
}

void pifPmlcdI2c_ScrollDisplayRight(PifPmlcdI2c* p_owner)
{
	_send(p_owner, LCD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_RIGHT, 0);
}

void pifPmlcdI2c_LeftToRight(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode |= LCD_ENTRY_LEFT;
	_send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

void pifPmlcdI2c_RightToLeft(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode &= ~LCD_ENTRY_LEFT;
	_send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

void pifPmlcdI2c_AutoScroll(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode |= LCD_ENTRY_SHIFT_INCREMENT;
	_send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

void pifPmlcdI2c_NoAutoScroll(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode &= ~LCD_ENTRY_SHIFT_INCREMENT;
	_send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

void pifPmlcdI2c_CreateChar(PifPmlcdI2c* p_owner, uint8_t location, uint8_t char_map[])
{
	location &= 0x7; // we only have 8 locations 0-7
	_send(p_owner, LCD_SET_CGRAM_ADDR | (location << 3), 0);
	for (int i = 0; i < 8; i++) {
		_send(p_owner, char_map[i], Rs);
	}
}

void pifPmlcdI2c_Backlight(PifPmlcdI2c* p_owner)
{
	p_owner->__backlight_val = LCD_BACK_LIGHT;
	_expanderWrite(p_owner, 0);
}

void pifPmlcdI2c_NoBacklight(PifPmlcdI2c* p_owner)
{
	p_owner->__backlight_val = LCD_NO_BACK_LIGHT;
	_expanderWrite(p_owner, 0);
}
