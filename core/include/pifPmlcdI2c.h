#ifndef PIF_PMLCD_I2C_H
#define PIF_PMLCD_I2C_H


#include "pifI2c.h"


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
	PifI2c _i2c;

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

PifPmlcdI2c* pifPmlcdI2c_Create(PifId id, uint8_t addr);
void pifPmlcdI2c_Destroy(PifPmlcdI2c** pp_owner);

void pifPmlcdI2c_Begin(PifPmlcdI2c* p_owner, uint8_t lines, uint8_t dot_size);

void pifPmlcdI2c_Print(PifPmlcdI2c* p_owner, const char* p_string);
void pifPmlcdI2c_Printf(PifPmlcdI2c* p_owner, const char* p_format, ...);

void pifPmlcdI2c_Clear(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_Home(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_SetCursor(PifPmlcdI2c* p_owner, uint8_t col, uint8_t row);

void pifPmlcdI2c_Display(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_NoDisplay(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_Cursor(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_NoCursor(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_Blink(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_NoBlink(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_ScrollDisplayLeft(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_ScrollDisplayRight(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_LeftToRight(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_RightToLeft(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_AutoScroll(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_NoAutoScroll(PifPmlcdI2c* p_owner);

void pifPmlcdI2c_CreateChar(PifPmlcdI2c* p_owner, uint8_t location, uint8_t char_map[]);

void pifPmlcdI2c_Backlight(PifPmlcdI2c* p_owner);
void pifPmlcdI2c_NoBacklight(PifPmlcdI2c* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PMLCD_I2C_H
