#ifndef PIF_TOUCH_SCREEN_H
#define PIF_TOUCH_SCREEN_H


#include "core/pif_task_manager.h"
#include "display/pif_tft_lcd.h"
#include "filter/pif_noise_filter.h"


#ifndef PIF_TOUCH_CONTROL_PERIOD
	#define PIF_TOUCH_CONTROL_PERIOD		10
#endif


struct StPifTouchScreen;
typedef struct StPifTouchScreen PifTouchScreen;

typedef void (*PifActTouchPosition)(PifTouchScreen* p_owner, int16_t* x, int16_t* y);
typedef BOOL (*PifActTouchPressure)(PifTouchScreen* p_owner);

typedef void (*PifEvtTouchData)(int16_t x, int16_t y);


/**
 * @class StPifTouchScreen
 * @brief Keypad 관리용 구조체
 */
struct StPifTouchScreen
{
	// Public Member Variable

	// Public Event Function
	PifEvtTouchData evt_touch_data;

	// Read-only Member Variable
	PifId _id;
    uint16_t _control_period_1ms;			// PIF_TOUCH_SCREEN_CONTROL_PERIOD
    PifTask* _p_task;
	int16_t _x, _y;
	BOOL _pressure;
	BOOL _calibration;

	// Private Member Variable
    PifTftLcd* __p_lcd;
	int16_t __clx, __cty, __crx, __cby;
	float __px, __py;
    PifNoiseFilter* __p_filter_x;
    PifNoiseFilter* __p_filter_y;

	// Private Action Function
	PifActTouchPosition __act_position;
	PifActTouchPressure __act_pressure;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTouchScreen_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_lcd
 * @param left_x
 * @param right_x
 * @param top_y
 * @param bottom_y
 * @return
 */
BOOL pifTouchScreen_Init(PifTouchScreen* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y);

/**
 * @fn pifTouchScreen_Clear
 * @brief
 * @param p_owner
 */
void pifTouchScreen_Clear(PifTouchScreen* p_owner);

/**
 * @fn pifTouchScreen_AttachAction
 * @brief
 * @param p_owner
 * @param act_position
 * @param act_pressure
 * @return
 */
BOOL pifTouchScreen_AttachAction(PifTouchScreen* p_owner, PifActTouchPosition act_position, PifActTouchPressure act_pressure);

/**
 * @fn pifTouchScreen_AttachFilter
 * @brief
 * @param p_owner
 * @param p_filter_x
 * @param p_filter_y
 * @return
 */
BOOL pifTouchScreen_AttachFilter(PifTouchScreen* p_owner, PifNoiseFilter* p_filter_x, PifNoiseFilter* p_filter_y);

/**
 * @fn pifTouchScreen_SetControlPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifTouchScreen_SetControlPeriod(PifTouchScreen* p_owner, uint16_t period1ms);

/**
 * @fn pifTouchScreen_Start
 * @brief
 * @param p_owner
 * @param p_name
 * @return
 */
BOOL pifTouchScreen_Start(PifTouchScreen* p_owner, const char* p_name);

/**
 * @fn pifTouchScreen_Stop
 * @brief
 * @param p_owner
 */
void pifTouchScreen_Stop(PifTouchScreen* p_owner);

/**
 * @fn pifTouchScreen_SetRotation
 * @brief
 * @param p_owner
 * @param rotation
 */
void pifTouchScreen_SetRotation(PifTouchScreen* p_owner, PifTftLcdRotation rotation);

/**
 * @fn pifTouchScreen_Calibration
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifTouchScreen_Calibration(PifTouchScreen* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_TOUCH_SCREEN_H
