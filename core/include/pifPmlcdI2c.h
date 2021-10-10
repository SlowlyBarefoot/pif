#ifndef PIF_PMLCD_I2C_H
#define PIF_PMLCD_I2C_H


#include "pifI2c.h"


#define PIF_PMLCD_DS_5x8			0x00
#define PIF_PMLCD_DS_5x10			0x04


/**
 * @class _PIF_stPmlcdI2c
 * @brief
 */
typedef struct _PIF_stPmlcdI2c
{
	// Public Member Variable

	// Read-only Member Variable
	PifI2c _stI2c;

	// Private Member Variable
	uint8_t __ucDisplayFunction;
	uint8_t __ucDisplayControl;
	uint8_t __ucDisplayMode;
	uint8_t __ucNumLines;
	uint8_t __ucBacklightVal;
} PIF_stPmlcdI2c;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stPmlcdI2c *pifPmlcdI2c_Create(PifId usPifId, uint8_t ucAddr);
void pifPmlcdI2c_Destroy(PIF_stPmlcdI2c **ppstOwner);

void pifPmlcdI2c_Begin(PIF_stPmlcdI2c *pstOwner, uint8_t ucLines, uint8_t ucDotSize);

void pifPmlcdI2c_Print(PIF_stPmlcdI2c *pstOwner, const char *pcString);
void pifPmlcdI2c_Printf(PIF_stPmlcdI2c *pstOwner, const char *pcFormat, ...);

void pifPmlcdI2c_Clear(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_Home(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_SetCursor(PIF_stPmlcdI2c *pstOwner, uint8_t ucCol, uint8_t ucRow);

void pifPmlcdI2c_Display(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_NoDisplay(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_Cursor(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_NoCursor(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_Blink(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_NoBlink(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_ScrollDisplayLeft(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_ScrollDisplayRight(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_LeftToRight(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_RightToLeft(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_AutoScroll(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_NoAutoScroll(PIF_stPmlcdI2c *pstOwner);

void pifPmlcdI2c_CreateChar(PIF_stPmlcdI2c *pstOwner,uint8_t ucLocation, uint8_t ucCharMap[]);

void pifPmlcdI2c_Backlight(PIF_stPmlcdI2c *pstOwner);
void pifPmlcdI2c_NoBacklight(PIF_stPmlcdI2c *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PMLCD_I2C_H
