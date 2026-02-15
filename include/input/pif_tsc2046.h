#ifndef PIF_TSC2046_H
#define PIF_TSC2046_H


#include "communication/pif_spi.h"
#include "input/pif_touch_screen.h"


typedef BOOL (*PifActTsc2046Pen)();

/**
 * @class StPifTsc2046
 * @brief TSC2046 touch controller context built on top of `PifTouchScreen`.
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
 * @brief Initializes a TSC2046 touch controller and binds SPI/pen callbacks.
 * @param p_owner Pointer to the TSC2046 instance to initialize.
 * @param id Instance ID. Use `PIF_ID_AUTO` to assign an ID automatically.
 * @param p_lcd Target LCD descriptor used for coordinate scaling.
 * @param left_x Raw touch value at the left edge.
 * @param right_x Raw touch value at the right edge.
 * @param top_y Raw touch value at the top edge.
 * @param bottom_y Raw touch value at the bottom edge.
 * @param p_port SPI port used to communicate with the TSC2046.
 * @param p_client User SPI client context passed to the SPI device.
 * @param act_pen Callback that returns the current pen interrupt state.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTsc2046_Init(PifTsc2046* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y, PifSpiPort* p_port, void *p_client, PifActTsc2046Pen act_pen);

/**
 * @fn pifTsc2046_Clear
 * @brief Removes the SPI device associated with the TSC2046 instance.
 * @param p_owner Pointer to the TSC2046 instance.
 */
void pifTsc2046_Clear(PifTsc2046* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_TSC2046_H
