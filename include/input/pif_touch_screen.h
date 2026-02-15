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
 * @brief Touch-screen controller context for coordinate conversion and polling.
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
 * @brief Initializes a touch-screen instance and calibration bounds.
 * @param p_owner Pointer to the touch-screen instance to initialize.
 * @param id Instance ID. Use `PIF_ID_AUTO` to assign an ID automatically.
 * @param p_lcd Target LCD descriptor used for coordinate scaling.
 * @param left_x Raw touch value at the left edge.
 * @param right_x Raw touch value at the right edge.
 * @param top_y Raw touch value at the top edge.
 * @param bottom_y Raw touch value at the bottom edge.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTouchScreen_Init(PifTouchScreen* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y);

/**
 * @fn pifTouchScreen_Clear
 * @brief Releases task resources used by the touch-screen instance.
 * @param p_owner Pointer to the touch-screen instance.
 */
void pifTouchScreen_Clear(PifTouchScreen* p_owner);

/**
 * @fn pifTouchScreen_AttachAction
 * @brief Attaches hardware callbacks for touch position and pressure sensing.
 * @param p_owner Pointer to the touch-screen instance.
 * @param act_position Callback that acquires raw touch coordinates.
 * @param act_pressure Callback that reports whether the panel is currently pressed.
 * @return `TRUE` if callbacks are valid and attached, otherwise `FALSE`.
 */
BOOL pifTouchScreen_AttachAction(PifTouchScreen* p_owner, PifActTouchPosition act_position, PifActTouchPressure act_pressure);

/**
 * @fn pifTouchScreen_AttachFilter
 * @brief Attaches optional noise filters for X and Y raw data streams.
 * @param p_owner Pointer to the touch-screen instance.
 * @param p_filter_x Filter applied to raw X values.
 * @param p_filter_y Filter applied to raw Y values.
 * @return `TRUE` if both filters are valid and attached, otherwise `FALSE`.
 */
BOOL pifTouchScreen_AttachFilter(PifTouchScreen* p_owner, PifNoiseFilter* p_filter_x, PifNoiseFilter* p_filter_y);

/**
 * @fn pifTouchScreen_SetControlPeriod
 * @brief Sets the periodic sampling interval of the touch task.
 * @param p_owner Pointer to the touch-screen instance.
 * @param period1ms New sampling period in milliseconds. Must be greater than zero.
 * @return `TRUE` if the period is valid and applied, otherwise `FALSE`.
 */
BOOL pifTouchScreen_SetControlPeriod(PifTouchScreen* p_owner, uint16_t period1ms);

/**
 * @fn pifTouchScreen_Start
 * @brief Starts or resumes periodic touch sampling.
 * @param p_owner Pointer to the touch-screen instance.
 * @param p_name Optional task name. If `NULL`, `"Touch"` is used.
 * @return `TRUE` if sampling starts successfully, otherwise `FALSE`.
 */
BOOL pifTouchScreen_Start(PifTouchScreen* p_owner, const char* p_name);

/**
 * @fn pifTouchScreen_Stop
 * @brief Pauses periodic touch sampling.
 * @param p_owner Pointer to the touch-screen instance.
 */
void pifTouchScreen_Stop(PifTouchScreen* p_owner);

/**
 * @fn pifTouchScreen_SetRotation
 * @brief Recomputes calibration parameters after display rotation changes.
 * @param p_owner Pointer to the touch-screen instance.
 * @param rotation Target display rotation.
 */
void pifTouchScreen_SetRotation(PifTouchScreen* p_owner, PifTftLcdRotation rotation);

/**
 * @fn pifTouchScreen_Calibration
 * @brief Runs interactive calibration and stores updated calibration values.
 * @param p_owner Pointer to the touch-screen instance.
 * @return `TRUE` if calibration completes with valid ranges, otherwise `FALSE`.
 */
BOOL pifTouchScreen_Calibration(PifTouchScreen* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_TOUCH_SCREEN_H
