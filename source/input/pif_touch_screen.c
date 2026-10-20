// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "input/pif_touch_screen.h"


#ifdef PIF_USE_TOUCH_CALIBRATION

/**
 * @brief Draws a calibration crosshair on the LCD.
 * @param p_owner Pointer to the LCD instance.
 * @param x Horizontal center coordinate.
 * @param y Vertical center coordinate.
 * @param color Drawing color for the crosshair.
 */
static void _drawCrossHair(PifTftLcd* p_owner, uint16_t x, uint16_t y, PifColor color)
{
	(*p_owner->_fn_draw_hor_line)(p_owner, x - 5, y, 11, color);
	(*p_owner->_fn_draw_ver_line)(p_owner, x, y - 5, 11, color);
}

/**
 * @brief Where the crosshair of a calibration point sits. The nine points of a 3x3 grid inset by
 *        ten pixels, without the centre one, in the order calibration asks for them.
 * @param p_owner Pointer to the touch-screen instance.
 * @param index Calibration point, 0 to PIF_TOUCH_CALIBRATION_POINTS - 1.
 * @param p_x Output pointer for the X coordinate.
 * @param p_y Output pointer for the Y coordinate.
 */
static void _calibrationPoint(PifTouchScreen* p_owner, uint8_t index, uint16_t* p_x, uint16_t* p_y)
{
	PifTftLcd* p_lcd = p_owner->__p_lcd;
	// The centre of the grid is point four and is not asked for, so everything from there on is
	// shifted past it.
	uint8_t n = index >= 4 ? index + 1 : index;

	*p_x = 10 + (n / 3) * ((p_lcd->_width - 20) / 2);
	*p_y = 10 + (n % 3) * ((p_lcd->_height - 20) / 2);
}

/**
 * @brief Reads the pressed state once and reports it when the reading has settled. One read
 *        happens per release of the task, so the reads are a control period apart.
 * @param p_owner Pointer to the touch-screen instance.
 * @param p_state Output pointer for the settled state. Untouched until it has settled.
 * @return `TRUE` once PIF_TOUCH_CALIBRATION_DEBOUNCE reads in a row have agreed.
 */
static BOOL _settledPressure(PifTouchScreen* p_owner, BOOL* p_state)
{
	BOOL state = (*p_owner->__act_pressure)(p_owner);

	if (state != p_owner->__cal_stable_state) {
		p_owner->__cal_stable_state = state;
		p_owner->__cal_stable_count = 1;
		return FALSE;
	}
	if (p_owner->__cal_stable_count < PIF_TOUCH_CALIBRATION_DEBOUNCE) {
		p_owner->__cal_stable_count++;
		if (p_owner->__cal_stable_count < PIF_TOUCH_CALIBRATION_DEBOUNCE) return FALSE;
	}

	*p_state = state;
	return TRUE;
}

/**
 * @brief Starts waiting for the pressed state to settle again, from no reading at all.
 * @param p_owner Pointer to the touch-screen instance.
 */
static void _restartSettling(PifTouchScreen* p_owner)
{
	p_owner->__cal_stable_count = 0;
	p_owner->__cal_stable_state = (*p_owner->__act_pressure)(p_owner);
}

/**
 * @brief Turns the collected samples into calibration values and reports the outcome. The values
 *        in use are replaced only when the new ones pass the range check.
 * @param p_owner Pointer to the touch-screen instance.
 * @param result TRUE when all points were sampled, FALSE when calibration gave up.
 */
static void _finishCalibration(PifTouchScreen* p_owner, BOOL result)
{
	PifTftLcd* p_lcd = p_owner->__p_lcd;
	uint16_t dispx = p_lcd->_width, dispy = p_lcd->_height;
	uint16_t* rx = p_owner->__cal_rx;
	uint16_t* ry = p_owner->__cal_ry;
#ifndef PIF_NO_LOG
	char* orientation[2] = { "PORTRAIT", "LANDSCAPE" };
#endif
	int16_t clx, cty, crx, cby;
	int16_t x_range, y_range, min_x, min_y;
	float px, py;

	if (!result) goto quit;

	// The three crosshairs down each edge, averaged, are what that edge reads as.
	clx = (rx[0] + rx[1] + rx[2]) / 3;
	crx = (rx[5] + rx[6] + rx[7]) / 3;
	cty = (ry[0] + ry[3] + ry[5]) / 3;
	cby = (ry[2] + ry[4] + ry[7]) / 3;
	px = (float)(crx - clx) / (dispx - 20);
	py = (float)(cby - cty) / (dispy - 20);
	// The crosshairs are inset by ten pixels, so the edges themselves lie that much further out.
	clx -= px * 10;
	crx += px * 10;
	cty -= py * 10;
	cby += py * 10;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "%s CALIBRATION : %d x %d", orientation[p_lcd->_rotation & 1], dispx, dispy);
	pifLog_Printf(LT_INFO, "px = %f, py = %f", px, py);
	pifLog_Printf(LT_INFO, "x = map(p.x, LEFT=%d, RT=%d, 0, %d)", clx, crx, dispx);
	pifLog_Printf(LT_INFO, "y = map(p.y, TOP=%d, BOT=%d, 0, %d)", cty, cby, dispy);
#endif

	// The minimum spans are given for the panel at TLR_0_DEGREE, and a quarter turn swaps which
	// panel axis the screen X runs along.
	if (p_lcd->_rotation & 1) {
		min_x = PIF_TOUCH_CALIBRATION_MIN_RANGE_Y;
		min_y = PIF_TOUCH_CALIBRATION_MIN_RANGE_X;
	}
	else {
		min_x = PIF_TOUCH_CALIBRATION_MIN_RANGE_X;
		min_y = PIF_TOUCH_CALIBRATION_MIN_RANGE_Y;
	}
	x_range = crx - clx;
	y_range = cby - cty;
	if (abs(x_range) < min_x || abs(y_range) < min_y) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "*** UNUSUAL CALIBRATION RANGES %d %d", x_range, y_range);
#endif
		result = FALSE;
		goto quit;
	}

	p_owner->__clx = clx;
	p_owner->__crx = crx;
	p_owner->__cty = cty;
	p_owner->__cby = cby;
	p_owner->__px = px;
	p_owner->__py = py;
	p_owner->_calibration = TRUE;

quit:
	(*p_lcd->_fn_draw_fill_rect)(p_lcd, 0, 0, dispx - 1, dispy - 1, PIF_COLOR_BLACK);
	p_owner->_calibration_state = TCS_IDLE;
	if (p_owner->__evt_calibration) (*p_owner->__evt_calibration)(p_owner, result);
}

/**
 * @brief Advances the calibration by one step. Called once per release of the task while a
 *        calibration is in progress, in place of the normal touch processing.
 * @param p_owner Pointer to the touch-screen instance.
 */
static void _processingCalibration(PifTouchScreen* p_owner)
{
	uint16_t x, y;
	int16_t tpx, tpy;
	uint16_t i;
	BOOL state;

	switch (p_owner->_calibration_state) {
	case TCS_WAIT_PRESS:
		if (!_settledPressure(p_owner, &state) || !state) break;

		p_owner->__cal_sum_x = 0UL;
		p_owner->__cal_sum_y = 0UL;
		p_owner->__cal_count = 0;
		p_owner->__cal_fail_count = 0;
		p_owner->__cal_miss_count = 0;
		p_owner->_calibration_state = TCS_SAMPLE;
		break;

	case TCS_SAMPLE:
		// A batch of reads rather than one, because a single sample per release would keep the user
		// in front of the same crosshair for PIF_TOUCH_CALIBRATION_SAMPLES control periods.
		for (i = 0; i < PIF_TOUCH_CALIBRATION_BATCH; i++) {
			if (p_owner->__p_lcd->_rotation & 1) {
				(*p_owner->__act_position)(p_owner, &tpy, &tpx);
			}
			else {
				(*p_owner->__act_position)(p_owner, &tpx, &tpy);
			}
			if ((*p_owner->__act_pressure)(p_owner)) {
				p_owner->__cal_sum_x += tpx;
				p_owner->__cal_sum_y += tpy;
				p_owner->__cal_count++;
				p_owner->__cal_miss_count = 0;
				if (p_owner->__cal_count >= PIF_TOUCH_CALIBRATION_SAMPLES) break;
			}
			else {
				p_owner->__cal_fail_count++;
				p_owner->__cal_miss_count++;
				if (p_owner->__cal_fail_count >= PIF_TOUCH_CALIBRATION_MAX_FAIL) break;
				if (p_owner->__cal_miss_count >= PIF_TOUCH_CALIBRATION_BATCH) break;
			}
		}

		if (p_owner->__cal_count >= PIF_TOUCH_CALIBRATION_SAMPLES) {
			p_owner->__cal_rx[p_owner->__cal_index] = p_owner->__cal_sum_x / p_owner->__cal_count;
			p_owner->__cal_ry[p_owner->__cal_index] = p_owner->__cal_sum_y / p_owner->__cal_count;
			_calibrationPoint(p_owner, p_owner->__cal_index, &x, &y);
			_drawCrossHair(p_owner->__p_lcd, x, y, PIF_COLOR_RED);
			_restartSettling(p_owner);
			p_owner->_calibration_state = TCS_WAIT_RELEASE;
		}
		else if (p_owner->__cal_fail_count >= PIF_TOUCH_CALIBRATION_MAX_FAIL) {
			// The contact kept dropping out, so this crosshair could not be sampled.
			_finishCalibration(p_owner, FALSE);
		}
		else if (p_owner->__cal_miss_count >= PIF_TOUCH_CALIBRATION_BATCH) {
			// The press was lifted before enough samples came in. Keeping them would mix them with
			// the next press, which need not land on the same spot, so this crosshair starts over.
			_restartSettling(p_owner);
			p_owner->_calibration_state = TCS_WAIT_PRESS;
		}
		break;

	case TCS_WAIT_RELEASE:
		if (!_settledPressure(p_owner, &state) || state) break;

		p_owner->__cal_index++;
		if (p_owner->__cal_index >= PIF_TOUCH_CALIBRATION_POINTS) {
			_finishCalibration(p_owner, TRUE);
			break;
		}
		_calibrationPoint(p_owner, p_owner->__cal_index, &x, &y);
		_drawCrossHair(p_owner->__p_lcd, x, y, PIF_COLOR_BLUE);
		_restartSettling(p_owner);
		p_owner->_calibration_state = TCS_WAIT_PRESS;
		break;

	default:
		break;
	}
}

#endif	// PIF_USE_TOUCH_CALIBRATION

/**
 * @brief Periodic touch task that acquires, filters, and converts touch data.
 * @param p_task Task context that contains a `PifTouchScreen` client pointer.
 * @return Always returns `0` to keep the periodic task active.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifTouchScreen* p_owner = p_task->_p_client;
	PifTftLcd* p_lcd = p_owner->__p_lcd;
	int16_t tpx, tpy;

#ifdef PIF_USE_TOUCH_CALIBRATION
	// While a calibration is running, the display and the panel belong to it and no touch data is
	// reported. It advances by one step per release, which is what makes the control period the
	// interval its debounce counts in.
	if (p_owner->_calibration_state != TCS_IDLE) {
		_processingCalibration(p_owner);
		return 0;
	}
#endif

	if (p_lcd->_rotation & 1) {
		(*p_owner->__act_position)(p_owner, &tpy, &tpx);
	}
	else {
		(*p_owner->__act_position)(p_owner, &tpx, &tpy);
	}
	if (!(*p_owner->__act_pressure)(p_owner)) {
		// The filters are emptied even when the press was too short for them to give a value,
		// or its samples would be averaged into the next press.
		if (p_owner->__filter_active) {
			pifNoiseFilter_Reset(p_owner->__p_filter_x);
			pifNoiseFilter_Reset(p_owner->__p_filter_y);
			p_owner->__filter_active = FALSE;
		}
		if (p_owner->_pressure) {
			p_owner->_pressure = FALSE;
			if (p_owner->evt_touch_released) (*p_owner->evt_touch_released)(p_owner->_x, p_owner->_y);
		}
		return 0;
	}

	if (p_owner->__p_filter_x && p_owner->__p_filter_y) {
		p_owner->__filter_active = TRUE;
		tpx = *(int16_t*)pifNoiseFilter_Process(p_owner->__p_filter_x, &tpx);
		tpy = *(int16_t*)pifNoiseFilter_Process(p_owner->__p_filter_y, &tpy);
		// The first samples of a press are noisy, so nothing is reported until the windows are full.
		if (!p_owner->__p_filter_x->_ready || !p_owner->__p_filter_y->_ready) return 0;
	}

	p_owner->_x = (tpx - p_owner->__clx) / p_owner->__px;
	if (p_owner->_x < 0) p_owner->_x = 0;
	else if (p_owner->_x >= p_lcd->_width) p_owner->_x = p_lcd->_width - 1;

	p_owner->_y = (tpy - p_owner->__cty) / p_owner->__py;
	if (p_owner->_y < 0) p_owner->_y = 0;
	else if (p_owner->_y >= p_lcd->_height) p_owner->_y = p_lcd->_height - 1;

	p_owner->_pressure = TRUE;

	if (p_owner->evt_touch_data) (*p_owner->evt_touch_data)(p_owner->_x, p_owner->_y);
	return 0;
}

BOOL pifTouchScreen_Init(PifTouchScreen* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y)
{
	// Equal edges would make the scale zero, and the task divides by it.
	if (!p_owner || !p_lcd || !p_lcd->_width || !p_lcd->_height || left_x == right_x || top_y == bottom_y) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTouchScreen));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_control_period_1ms = PIF_TOUCH_CONTROL_PERIOD;
	p_owner->__p_lcd = p_lcd;

	p_owner->__clx = left_x;
	p_owner->__crx = right_x;
	p_owner->__cty = top_y;
	p_owner->__cby = bottom_y;

	p_owner->__px = (float)(p_owner->__crx - p_owner->__clx) / p_lcd->_width;
	p_owner->__py = (float)(p_owner->__cby - p_owner->__cty) / p_lcd->_height;
	return TRUE;
}

void pifTouchScreen_Clear(PifTouchScreen* p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
#ifdef PIF_USE_TOUCH_CALIBRATION
	p_owner->_calibration_state = TCS_IDLE;
#endif
}

BOOL pifTouchScreen_AttachAction(PifTouchScreen* p_owner, PifActTouchPosition act_position, PifActTouchPressure act_pressure)
{
	if (!p_owner || !act_position || !act_pressure) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__act_position = act_position;
	p_owner->__act_pressure = act_pressure;
	return TRUE;
}

BOOL pifTouchScreen_AttachFilter(PifTouchScreen* p_owner, PifNoiseFilter* p_filter_x, PifNoiseFilter* p_filter_y)
{
	if (!p_owner || !p_filter_x || !p_filter_y || p_filter_x->_value != NFV_INT16 || p_filter_y->_value != NFV_INT16) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__p_filter_x = p_filter_x;
	p_owner->__p_filter_y = p_filter_y;
	p_owner->__filter_active = FALSE;
	return TRUE;
}

BOOL pifTouchScreen_SetControlPeriod(PifTouchScreen* p_owner, uint16_t period1ms)
{
	if (!p_owner || !period1ms) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_control_period_1ms = period1ms;
	if (p_owner->_p_task) {
		pifTask_ChangePeriod(p_owner->_p_task, p_owner->_control_period_1ms * 1000UL);
	}
	return TRUE;
}

BOOL pifTouchScreen_Start(PifTouchScreen* p_owner, const char* p_name)
{
	// The task calls both actions on its first release.
	if (!p_owner || !p_owner->__act_position || !p_owner->__act_pressure) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_owner->_p_task) pifTask_Resume(p_owner->_p_task);
	else {
		p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, p_owner->_control_period_1ms * 1000UL, _doTask, p_owner, TRUE);
		if (!p_owner->_p_task) return FALSE;
		p_owner->_p_task->name = p_name ? p_name : "Touch";
	}
	return TRUE;
}

void pifTouchScreen_Stop(PifTouchScreen* p_owner)
{
	if (p_owner && p_owner->_p_task) pifTask_Pause(p_owner->_p_task);
}

BOOL pifTouchScreen_SetRotation(PifTouchScreen* p_owner, PifTftLcdRotation rotation)
{
	PifTftLcd* p_lcd;
	int16_t clx, cty, crx, cby;
	float px, py;

	if (!p_owner || rotation > TLR_270_DEGREE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
#ifdef PIF_USE_TOUCH_CALIBRATION
	if (p_owner->_calibration_state != TCS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
#endif

	p_lcd = p_owner->__p_lcd;

	clx = p_owner->__clx;	cty = p_owner->__cty;
	crx = p_owner->__crx;	cby = p_owner->__cby;
	px = p_owner->__px;		py = p_owner->__py;
	switch (p_lcd->_rotation) {
	case TLR_90_DEGREE:
		p_owner->__clx = cby;	p_owner->__cty = clx;
		p_owner->__crx = cty;	p_owner->__cby = crx;
		p_owner->__px = -py;	p_owner->__py = px;
		break;

	case TLR_180_DEGREE:
		p_owner->__clx = crx;	p_owner->__cty = cby;
		p_owner->__crx = clx;	p_owner->__cby = cty;
		p_owner->__px = -px;	p_owner->__py = -py;
		break;

	case TLR_270_DEGREE:
		p_owner->__clx = cty;	p_owner->__cty = crx;
		p_owner->__crx = cby;	p_owner->__cby = clx;
		p_owner->__px = py;		p_owner->__py = -px;
		break;

	default:
		break;
	}

	if (p_lcd->_fn_set_rotation) (*p_lcd->_fn_set_rotation)(p_lcd, rotation);

	clx = p_owner->__clx;	cty = p_owner->__cty;
	crx = p_owner->__crx;	cby = p_owner->__cby;
	px = p_owner->__px;		py = p_owner->__py;
	switch (p_lcd->_rotation) {
	case TLR_90_DEGREE:
		p_owner->__clx = cty;	p_owner->__cty = crx;
		p_owner->__crx = cby;	p_owner->__cby = clx;
		p_owner->__px = py;		p_owner->__py = -px;
		break;

	case TLR_180_DEGREE:
		p_owner->__clx = crx;	p_owner->__cty = cby;
		p_owner->__crx = clx;	p_owner->__cby = cty;
		p_owner->__px = -px;	p_owner->__py = -py;
		break;

	case TLR_270_DEGREE:
		p_owner->__clx = cby;	p_owner->__cty = clx;
		p_owner->__crx = cty;	p_owner->__cby = crx;
		p_owner->__px = -py;	p_owner->__py = px;
		break;

	default:
		break;
	}

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "px = %f, py = %f", p_owner->__px, p_owner->__py);
	pifLog_Printf(LT_INFO, "x = map(p.x, LEFT=%d, RIGHT=%d, 0, %d)", p_owner->__clx, p_owner->__crx, p_lcd->_width);
	pifLog_Printf(LT_INFO, "y = map(p.y, TOP=%d, BOTTOM=%d, 0, %d)", p_owner->__cty, p_owner->__cby, p_lcd->_height);
#endif
	return TRUE;
}

#ifdef PIF_USE_TOUCH_CALIBRATION

BOOL pifTouchScreen_StartCalibration(PifTouchScreen* p_owner, PifEvtTouchCalibration evt_calibration)
{
	PifTftLcd* p_lcd;
	uint16_t x, y;
	uint8_t i;

	if (!p_owner || !p_owner->__p_lcd || !p_owner->__act_position || !p_owner->__act_pressure) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	// Nothing would move the calibration on without the task, so a calibration that cannot make
	// progress is refused rather than left waiting.
	if (!p_owner->_p_task || p_owner->_p_task->_pause) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (p_owner->_calibration_state != TCS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_lcd = p_owner->__p_lcd;
	(*p_lcd->_fn_draw_fill_rect)(p_lcd, 0, 0, p_lcd->_width - 1, p_lcd->_height - 1, PIF_COLOR_BLACK);

	// All of the crosshairs at once. They used to be drawn a tenth of a second apart, which only
	// spaced out the drawing itself.
	for (i = 0; i < PIF_TOUCH_CALIBRATION_POINTS; i++) {
		_calibrationPoint(p_owner, i, &x, &y);
		_drawCrossHair(p_lcd, x, y, PIF_COLOR_GRAY);
	}

	p_owner->__evt_calibration = evt_calibration;
	p_owner->__cal_index = 0;
	_calibrationPoint(p_owner, 0, &x, &y);
	_drawCrossHair(p_lcd, x, y, PIF_COLOR_BLUE);
	_restartSettling(p_owner);
	p_owner->_calibration_state = TCS_WAIT_PRESS;
	return TRUE;
}

BOOL pifTouchScreen_CancelCalibration(PifTouchScreen* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_calibration_state == TCS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	_finishCalibration(p_owner, FALSE);
	return TRUE;
}

#endif
