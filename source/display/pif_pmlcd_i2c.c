#include <string.h>

#include "core/pif_task.h"
#include "display/pif_pmlcd_i2c.h"


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
	uint8_t tmp = data | p_owner->__backlight_val;
	return pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, &tmp, 1);
}

static BOOL _pulseEnable(PifPmlcdI2c* p_owner, uint8_t data)
{
	if (!_expanderWrite(p_owner, data | En)) return FALSE;	// En high
	pif_Delay1us(1);										// enable pulse must be >450ns

	if (!_expanderWrite(p_owner, data & ~En)) return FALSE;	// En low
	pifTaskManager_YieldUs(50);								// commands need > 37us to settle
	return TRUE;
}

static BOOL _write4bits(PifPmlcdI2c* p_owner, uint8_t value)
{
	if (!_expanderWrite(p_owner, value)) return FALSE;
	if (!_pulseEnable(p_owner, value)) return FALSE;
	return TRUE;
}

// write either command or data
static BOOL _send(PifPmlcdI2c* p_owner, uint8_t value, uint8_t mode)
{
    if (!_write4bits(p_owner, (value & 0xf0) | mode)) return FALSE;
	if (!_write4bits(p_owner, ((value << 4) & 0xf0) | mode)) return FALSE;
	return TRUE;
}

BOOL pifPmlcdI2c_Init(PifPmlcdI2c* p_owner, PifId id, PifI2cPort* p_port, uint8_t addr)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifPmlcdI2c));

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->_p_i2c = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr);
    if (!p_owner->_p_i2c) goto fail;

    p_owner->__backlight_val = LCD_NO_BACK_LIGHT;
    p_owner->__display_function = LCD_4BIT_MODE | LCD_1LINE | LCD_5x8_DOTS;
    return TRUE;

fail:
	pifPmlcdI2c_Clear(p_owner);
    return FALSE;
}

void pifPmlcdI2c_Clear(PifPmlcdI2c* p_owner)
{
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
	}
}

BOOL pifPmlcdI2c_Begin(PifPmlcdI2c* p_owner, uint8_t lines, uint8_t dot_size)
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
	if (!_expanderWrite(p_owner, p_owner->__backlight_val)) return FALSE;	// reset expanderand turn backlight off (Bit 8 =1)
	pifTaskManager_YieldMs(1000);

  	//put the LCD into 4 bit mode
	// this is according to the hitachi HD44780 datasheet
	// figure 24, pg 46

	// we start in 8bit mode, try to set 4 bit mode
	if (!_write4bits(p_owner, 0x03 << 4)) return FALSE;
	pifTaskManager_YieldUs(4500); // wait min 4.1ms

	// second try
	if (!_write4bits(p_owner, 0x03 << 4)) return FALSE;
	pifTaskManager_YieldUs(4500); // wait min 4.1ms

	// third go!
	if (!_write4bits(p_owner, 0x03 << 4)) return FALSE;
	pifTaskManager_YieldUs(150);

	// finally, set to 4-bit interface
	if (!_write4bits(p_owner, 0x02 << 4)) return FALSE;

	// set # lines, font size, etc.
	if (!_send(p_owner, LCD_FUNCTION_SET | p_owner->__display_function, 0)) return FALSE;

	// turn the display on with no cursor or blinking default
	p_owner->__display_control = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
	if (!pifPmlcdI2c_Display(p_owner)) return FALSE;

	// clear it off
	if (!pifPmlcdI2c_DisplayClear(p_owner)) return FALSE;

	// Initialize to default text direction (for roman languages)
	p_owner->__display_mode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DECREMENT;

	// set the entry mode
	if (!_send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0)) return FALSE;

	if (!pifPmlcdI2c_Home(p_owner)) return FALSE;
	return TRUE;
}

BOOL pifPmlcdI2c_Print(PifPmlcdI2c* p_owner, const char* p_string)
{
	uint8_t i;

	for (i = 0; i < strlen(p_string); i++) {
		if (!_send(p_owner, p_string[i], Rs)) return FALSE;
	}
	return TRUE;
}

BOOL pifPmlcdI2c_Printf(PifPmlcdI2c* p_owner, const char* p_format, ...)
{
	va_list data;
	char buffer[32];

	va_start(data, p_format);
	pif_PrintFormat(buffer, sizeof(buffer), &data, p_format);
	va_end(data);

	return pifPmlcdI2c_Print(p_owner, buffer);
}

BOOL pifPmlcdI2c_DisplayClear(PifPmlcdI2c* p_owner)
{
	if (!_send(p_owner, LCD_CLEAR_DISPLAY, 0)) return FALSE;// clear display, set cursor position to zero
	pifTaskManager_YieldMs(2);  							// this command takes a long time!
	return TRUE;
}

BOOL pifPmlcdI2c_Home(PifPmlcdI2c* p_owner)
{
	if (!_send(p_owner, LCD_RETURN_HOME, 0)) return FALSE;  // set cursor position to zero
	pifTaskManager_YieldMs(2);  							// this command takes a long time!
	return TRUE;
}

BOOL pifPmlcdI2c_SetCursor(PifPmlcdI2c* p_owner, uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };

	if (row > p_owner->__num_lines) {
		row = p_owner->__num_lines - 1;    // we count rows starting w/0
	}
	return _send(p_owner, LCD_SET_DDRAM_ADDR | (col + row_offsets[row]), 0);
}

BOOL pifPmlcdI2c_Display(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control |= LCD_DISPLAY_ON;
	return _send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

BOOL pifPmlcdI2c_NoDisplay(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control &= ~LCD_DISPLAY_ON;
	return _send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

BOOL pifPmlcdI2c_Cursor(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control |= LCD_CURSOR_ON;
	return _send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

BOOL pifPmlcdI2c_NoCursor(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control &= ~LCD_CURSOR_ON;
	return _send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

BOOL pifPmlcdI2c_Blink(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control |= LCD_BLINK_ON;
	return _send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

BOOL pifPmlcdI2c_NoBlink(PifPmlcdI2c* p_owner)
{
	p_owner->__display_control &= ~LCD_BLINK_ON;
	return _send(p_owner, LCD_DISPLAY_CONTROL | p_owner->__display_control, 0);
}

BOOL pifPmlcdI2c_ScrollDisplayLeft(PifPmlcdI2c* p_owner)
{
	return _send(p_owner, LCD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_LEFT, 0);
}

BOOL pifPmlcdI2c_ScrollDisplayRight(PifPmlcdI2c* p_owner)
{
	return _send(p_owner, LCD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_RIGHT, 0);
}

BOOL pifPmlcdI2c_LeftToRight(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode |= LCD_ENTRY_LEFT;
	return _send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

BOOL pifPmlcdI2c_RightToLeft(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode &= ~LCD_ENTRY_LEFT;
	return _send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

BOOL pifPmlcdI2c_AutoScroll(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode |= LCD_ENTRY_SHIFT_INCREMENT;
	return _send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

BOOL pifPmlcdI2c_NoAutoScroll(PifPmlcdI2c* p_owner)
{
	p_owner->__display_mode &= ~LCD_ENTRY_SHIFT_INCREMENT;
	return _send(p_owner, LCD_ENTRY_MODE_SET | p_owner->__display_mode, 0);
}

BOOL pifPmlcdI2c_CreateChar(PifPmlcdI2c* p_owner, uint8_t location, uint8_t char_map[])
{
	location &= 0x7; // we only have 8 locations 0-7
	if (!_send(p_owner, LCD_SET_CGRAM_ADDR | (location << 3), 0)) return FALSE;
	for (int i = 0; i < 8; i++) {
		if (!_send(p_owner, char_map[i], Rs)) return FALSE;
	}
	return TRUE;
}

BOOL pifPmlcdI2c_Backlight(PifPmlcdI2c* p_owner)
{
	p_owner->__backlight_val = LCD_BACK_LIGHT;
	return _expanderWrite(p_owner, 0);
}

BOOL pifPmlcdI2c_NoBacklight(PifPmlcdI2c* p_owner)
{
	p_owner->__backlight_val = LCD_NO_BACK_LIGHT;
	return _expanderWrite(p_owner, 0);
}
