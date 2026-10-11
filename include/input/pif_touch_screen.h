// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_TOUCH_SCREEN_H
#define PIF_TOUCH_SCREEN_H


#include "core/pif_task_manager.h"
#include "display/pif_tft_lcd.h"
#include "filter/pif_noise_filter.h"


#ifndef PIF_TOUCH_CONTROL_PERIOD
	#define PIF_TOUCH_CONTROL_PERIOD		10
#endif

// Reads of the pressed state that have to agree before calibration accepts a press or a release.
// One read happens per release of the task, so at the default control period this is the 100ms
// the blocking calibration used to spend on it.
#ifndef PIF_TOUCH_CALIBRATION_DEBOUNCE
	#define PIF_TOUCH_CALIBRATION_DEBOUNCE	10
#endif

// Samples of one crosshair that calibration accepts before it moves on to the next.
#ifndef PIF_TOUCH_CALIBRATION_SAMPLES
	#define PIF_TOUCH_CALIBRATION_SAMPLES	400
#endif

// Reads calibration takes per release of the task while sampling a crosshair. The whole batch is
// one uninterrupted run, so it trades how long calibration takes against how long it holds the
// CPU: 400 samples need PIF_TOUCH_CALIBRATION_SAMPLES / this many releases. This many unpressed
// reads in a row also count as the press having been lifted, which discards the samples taken so
// far and waits for the crosshair to be pressed again.
#ifndef PIF_TOUCH_CALIBRATION_BATCH
	#define PIF_TOUCH_CALIBRATION_BATCH		40
#endif

// Unpressed reads scattered through the samples of one crosshair that make calibration give up,
// because the contact is too unsteady to sample.
#ifndef PIF_TOUCH_CALIBRATION_MAX_FAIL
	#define PIF_TOUCH_CALIBRATION_MAX_FAIL	10000
#endif

// Smallest raw span between the edges that calibration accepts, along the X and Y axes of the
// panel at TLR_0_DEGREE. A smaller span means the presses were not where the crosshairs were.
// The defaults suit a 10-bit reading such as the ADC of an Arduino.
#ifndef PIF_TOUCH_CALIBRATION_MIN_RANGE_X
	#define PIF_TOUCH_CALIBRATION_MIN_RANGE_X	500
#endif
#ifndef PIF_TOUCH_CALIBRATION_MIN_RANGE_Y
	#define PIF_TOUCH_CALIBRATION_MIN_RANGE_Y	650
#endif

// Crosshairs calibration asks for: the nine points of a 3x3 grid without its centre.
#define PIF_TOUCH_CALIBRATION_POINTS		8


struct StPifTouchScreen;
typedef struct StPifTouchScreen PifTouchScreen;

typedef void (*PifActTouchPosition)(PifTouchScreen* p_owner, int16_t* x, int16_t* y);
typedef BOOL (*PifActTouchPressure)(PifTouchScreen* p_owner);

typedef void (*PifEvtTouchData)(int16_t x, int16_t y);

/**
 * @brief Reports the outcome of a calibration that pifTouchScreen_StartCalibration() began.
 * @param p_owner Pointer to the touch-screen instance.
 * @param result TRUE when the calibration finished with usable ranges.
 */
typedef void (*PifEvtTouchCalibration)(PifTouchScreen* p_owner, BOOL result);

/**
 * @enum EnPifTouchCalibrationState
 * @brief How far the calibration started by pifTouchScreen_StartCalibration() has got. It is
 *        advanced by the touch task, one step per release, so the task has to be running for it
 *        to move at all.
 */
typedef enum EnPifTouchCalibrationState
{
	TCS_IDLE = 0,			// Not calibrating: the task does its normal work
	TCS_WAIT_PRESS,			// Waiting for the crosshair in front of the user to be pressed
	TCS_SAMPLE,				// Collecting samples of that press
	TCS_WAIT_RELEASE		// Waiting for the press to be released again
} PifTouchCalibrationState;


/**
 * @class StPifTouchScreen
 * @brief Touch-screen controller context for coordinate conversion and polling.
 */
struct StPifTouchScreen
{
	// Public Member Variable

	// Public Event Function
	PifEvtTouchData evt_touch_data;			// Every converted position while the panel is pressed
	PifEvtTouchData evt_touch_released;		// Once when the panel is let go, with the last position

	// Read-only Member Variable
	PifId _id;
	uint16_t _control_period_1ms;			// PIF_TOUCH_CONTROL_PERIOD
	PifTask* _p_task;
	int16_t _x, _y;
	BOOL _pressure;
	BOOL _calibration;						// TRUE when the values in use came from a calibration
											// rather than from pifTouchScreen_Init()
	PifTouchCalibrationState _calibration_state;

	// Private Member Variable
	PifTftLcd* __p_lcd;
	int16_t __clx, __cty, __crx, __cby;
	float __px, __py;
	PifNoiseFilter* __p_filter_x;
	PifNoiseFilter* __p_filter_y;
	BOOL __filter_active;					// The filters hold samples of the press in progress
	uint16_t __cal_rx[PIF_TOUCH_CALIBRATION_POINTS];
	uint16_t __cal_ry[PIF_TOUCH_CALIBRATION_POINTS];
	uint32_t __cal_sum_x, __cal_sum_y;
	uint16_t __cal_count;					// Accepted samples of the crosshair in progress
	uint16_t __cal_fail_count;
	uint16_t __cal_miss_count;				// Unpressed reads in a row while sampling
	uint16_t __cal_stable_count;			// Reads of the pressed state that have agreed so far
	uint8_t __cal_index;					// Which crosshair is in front of the user
	BOOL __cal_stable_state;

	// Private Action Function
	PifActTouchPosition __act_position;
	PifActTouchPressure __act_pressure;

	// Private Event Function
	PifEvtTouchCalibration __evt_calibration;
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
 * @param right_x Raw touch value at the right edge. Must differ from left_x.
 * @param top_y Raw touch value at the top edge.
 * @param bottom_y Raw touch value at the bottom edge. Must differ from top_y.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTouchScreen_Init(PifTouchScreen* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y);

/**
 * @fn pifTouchScreen_Clear
 * @brief Releases task resources used by the touch-screen instance. A calibration in progress is
 *        dropped without calling its callback.
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
 * @return `TRUE` if sampling starts successfully, otherwise `FALSE`. It fails with
 *         `E_INVALID_PARAM` until pifTouchScreen_AttachAction() has been called.
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
 * @brief Rotates the display and recomputes calibration parameters to match. Rotate the display
 *        through this rather than through the LCD, or touch positions no longer line up.
 * @param p_owner Pointer to the touch-screen instance.
 * @param rotation Target display rotation.
 * @return `TRUE` if applied. `FALSE` with `E_INVALID_STATE` while a calibration is in progress,
 *         since its crosshairs and samples belong to the current rotation.
 */
BOOL pifTouchScreen_SetRotation(PifTouchScreen* p_owner, PifTftLcdRotation rotation);

/**
 * @fn pifTouchScreen_StartCalibration
 * @brief Begins interactive calibration and returns at once. Eight crosshairs are drawn and the
 *        user is asked for one press on each; the touch task advances that a step per release and
 *        reports the outcome through the callback, so the task has to be running and the display
 *        belongs to calibration until it finishes. No touch data is reported meanwhile.
 *        Watch `_calibration_state` to see how far it has got, or wait for the callback.
 *        The calibration in use is replaced only when the new one succeeds.
 * @param p_owner Pointer to the touch-screen instance.
 * @param evt_calibration Called once with the result. May be NULL.
 * @return `TRUE` if calibration was started, otherwise `FALSE`.
 */
BOOL pifTouchScreen_StartCalibration(PifTouchScreen* p_owner, PifEvtTouchCalibration evt_calibration);

/**
 * @fn pifTouchScreen_CancelCalibration
 * @brief Abandons a calibration in progress. The display is cleared and handed back, the
 *        calibration in use is kept, and the callback is called with `FALSE`.
 * @param p_owner Pointer to the touch-screen instance.
 * @return `TRUE` if a calibration was cancelled, `FALSE` with `E_INVALID_STATE` if none was running.
 */
BOOL pifTouchScreen_CancelCalibration(PifTouchScreen* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_TOUCH_SCREEN_H
