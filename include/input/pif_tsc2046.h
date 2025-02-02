#ifndef PIF_TSC2046_H
#define PIF_TSC2046_H


#include "communication/pif_spi.h"
#include "input/pif_touch_screen.h"


typedef BOOL (*PifActTsc2046Pen)();

/**
 * @class StPifTsc2046
 * @brief Keypad 관리용 구조체
 */
typedef struct StPifTsc2046
{
	// The parent variable must be at the beginning of this structure.
	PifTouchScreen parent;

	// Public Member Variable
	PifSpiDevice* _p_spi;

	// Public Event Function

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable

	// Private Action Function
	PifActTsc2046Pen __act_pen;
} PifTsc2046;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTsc2046_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_lcd
 * @param left_x
 * @param right_x
 * @param top_y
 * @param bottom_y
 * @param p_port
 * @param p_client
 * @param act_pen
 * @return
 */
BOOL pifTsc2046_Init(PifTsc2046* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y, PifSpiPort* p_port, void *p_client, PifActTsc2046Pen act_pen);

/**
 * @fn pifTsc2046_Clear
 * @brief
 * @param p_owner
 */
void pifTsc2046_Clear(PifTsc2046* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_TSC2046_H
