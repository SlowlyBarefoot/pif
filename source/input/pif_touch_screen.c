#include "core/pif_log.h"
#include "input/pif_touch_screen.h"


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
 * @brief Reads pressure repeatedly until the state is stable.
 * @param p_owner Pointer to the touch-screen instance.
 * @return Stable pressed state (`TRUE` when pressed).
 */
static BOOL _isPressed(PifTouchScreen* p_owner)
{
    int count = 0;
    BOOL state, oldstate = FALSE;

    while (count < 10) {
        state = (*p_owner->__act_pressure)(p_owner);
        if (state == oldstate) count++;
        else count = 0;
        oldstate = state;
		pifTaskManager_YieldMs(PIF_TOUCH_CONTROL_PERIOD);
    }
    return oldstate;
}

/**
 * @brief Performs one calibration sampling step at a target screen point.
 * @param p_owner Pointer to the touch-screen instance.
 * @param x Target X coordinate for user touch.
 * @param y Target Y coordinate for user touch.
 * @param p_rx Output pointer for averaged raw X value.
 * @param p_ry Output pointer for averaged raw Y value.
 * @return `TRUE` if a valid sample is collected, otherwise `FALSE`.
 */
static BOOL _calibrate(PifTouchScreen* p_owner, uint16_t x, uint16_t y, uint16_t* p_rx, uint16_t* p_ry)
{
    int iter = 5000;
    int failcount = 0;
    int cnt = 0;
	int16_t tpx, tpy;
    uint32_t tx = 0, ty = 0;
    BOOL OK = FALSE;

	_drawCrossHair(p_owner->__p_lcd, x, y, BLUE);

    while (OK == FALSE) {
        while (_isPressed(p_owner) == FALSE) pifTaskManager_Yield();
        cnt = 0;
        iter = 400;
        do {
            if (p_owner->__p_lcd->_rotation & 1) {
            	(*p_owner->__act_position)(p_owner, &tpy, &tpx);
            }
            else {
            	(*p_owner->__act_position)(p_owner, &tpx, &tpy);
            }
            if ((*p_owner->__act_pressure)(p_owner)) {
                tx += tpx;
                ty += tpy;
                cnt++;
            }
            else failcount++;
			pifTaskManager_Yield();
        } while ((cnt < iter) && (failcount < 10000));
        if (cnt >= iter) OK = TRUE;
        else {
            tx = 0;
            ty = 0;
            cnt = 0;
        }
        if (failcount >= 10000) return FALSE;
    }

    *p_rx = tx / cnt;
    *p_ry = ty / cnt;

    _drawCrossHair(p_owner->__p_lcd, x, y, RED);

    while (_isPressed(p_owner) == TRUE) pifTaskManager_Yield();
    return TRUE;
}

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
	PifNoiseFilterValueP p_vx, p_vy;

	if (p_lcd->_rotation & 1) {
		(*p_owner->__act_position)(p_owner, &tpy, &tpx);
	}
	else {
		(*p_owner->__act_position)(p_owner, &tpx, &tpy);
	}
	if (!(*p_owner->__act_pressure)(p_owner)) {
		if (p_owner->_pressure) {
			if (p_owner->__p_filter_x && p_owner->__p_filter_y) {
				pifNoiseFilter_Reset(p_owner->__p_filter_x);
				pifNoiseFilter_Reset(p_owner->__p_filter_y);
			}
			p_owner->_pressure = FALSE;
		}
		return 0;
	}

    if (p_owner->__p_filter_x && p_owner->__p_filter_y) {
    	p_vx = pifNoiseFilter_Process(p_owner->__p_filter_x, &tpx);
    	p_vy = pifNoiseFilter_Process(p_owner->__p_filter_y, &tpy);
    	if (!p_vx || !p_vy) return 0;
    	tpx = *(int16_t*)p_vx;
    	tpy = *(int16_t*)p_vy;
    }

    p_owner->_x = (tpx - p_owner->__clx) / p_owner->__px;
	if (p_owner->_x < 0) p_owner->_x = 0;
	else if (p_owner->_x > p_lcd->_width) p_owner->_x = p_lcd->_width - 1;

	p_owner->_y = (tpy - p_owner->__cty) / p_owner->__py;
	if (p_owner->_y < 0) p_owner->_y = 0;
	else if (p_owner->_y > p_lcd->_height) p_owner->_y = p_lcd->_height - 1;

	p_owner->_pressure = TRUE;

	if (p_owner->evt_touch_data) (*p_owner->evt_touch_data)(p_owner->_x, p_owner->_y);
	return 0;
}

BOOL pifTouchScreen_Init(PifTouchScreen* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y)
{
	if (!p_owner || !p_lcd) {
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

	p_owner->__px = (p_owner->__crx - p_owner->__clx) / p_lcd->_width;
	p_owner->__py = (p_owner->__cby - p_owner->__cty) / p_lcd->_height;
    return TRUE;
}

void pifTouchScreen_Clear(PifTouchScreen* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifTouchScreen_AttachAction(PifTouchScreen* p_owner, PifActTouchPosition act_position, PifActTouchPressure act_pressure)
{
	if (!p_owner || !act_position || ! act_pressure) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__act_position = act_position;
	p_owner->__act_pressure = act_pressure;
    return TRUE;
}

BOOL pifTouchScreen_AttachFilter(PifTouchScreen* p_owner, PifNoiseFilter* p_filter_x, PifNoiseFilter* p_filter_y)
{
    if (!p_filter_x || !p_filter_y) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    p_owner->__p_filter_x = p_filter_x;
	p_owner->__p_filter_y = p_filter_y;
	return TRUE;
}

BOOL pifTouchScreen_SetControlPeriod(PifTouchScreen* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_owner->_control_period_1ms = period1ms;
   	pifTask_ChangePeriod(p_owner->_p_task, p_owner->_control_period_1ms);
	return TRUE;
}

BOOL pifTouchScreen_Start(PifTouchScreen* p_owner, const char* p_name)
{
	if (p_owner->_p_task) p_owner->_p_task->pause = FALSE;
	else {
		p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, p_owner->_control_period_1ms * 1000, _doTask, p_owner, TRUE);
		if (!p_owner->_p_task) return FALSE;
		p_owner->_p_task->name = p_name ? p_name : "Touch";
	}
	return TRUE;
}

void pifTouchScreen_Stop(PifTouchScreen* p_owner)
{
	if (p_owner->_p_task) p_owner->_p_task->pause = TRUE;
}

void pifTouchScreen_SetRotation(PifTouchScreen* p_owner, PifTftLcdRotation rotation)
{
	PifTftLcd* p_lcd = p_owner->__p_lcd;
	int16_t clx, cty, crx, cby;
	float px, py;

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

	(*p_lcd->_fn_set_rotation)(p_lcd, rotation);

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
}

BOOL pifTouchScreen_Calibration(PifTouchScreen* p_owner)
{
	PifTftLcd* p_lcd = p_owner->__p_lcd;
	char* orientation[2] = { "PORTRAIT", "LANDSCAPE" };
	uint8_t cnt, idx;
	uint16_t x, y, dispx = p_lcd->_width, dispy = p_lcd->_height;
	uint16_t rx[8], ry[8];
	BOOL rtn = TRUE;

	(*p_lcd->_fn_draw_fill_rect)(p_lcd, 0, 0, dispx, dispy, BLACK);

	for (x = 10, cnt = 0; x < dispx; x += (dispx - 20) / 2) {
        for (y = 10; y < dispy; y += (dispy - 20) / 2) {
            if (++cnt != 5) _drawCrossHair(p_lcd, x, y, GRAY);
			pifTaskManager_YieldMs(10);
        }
    }
    for (x = 10, cnt = 0, idx = 0; x < dispx; x += (dispx - 20) / 2) {
        for (y = 10; y < dispy; y += (dispy - 20) / 2) {
            if (++cnt != 5) {
            	if (!_calibrate(p_owner, x, y, &rx[idx], &ry[idx])) {
            		rtn = FALSE;
            		goto fail;
            	}
            	idx++;
            }
			pifTaskManager_YieldMs(10);
        }
    }

    p_owner->__clx = (rx[0] + rx[1] + rx[2]) / 3;
    p_owner->__crx = (rx[5] + rx[6] + rx[7]) / 3;
	p_owner->__cty = (ry[0] + ry[3] + ry[5]) / 3;
	p_owner->__cby = (ry[2] + ry[4] + ry[7]) / 3;
	p_owner->__px = (float)(p_owner->__crx - p_owner->__clx) / (dispx - 20);
	p_owner->__py = (float)(p_owner->__cby - p_owner->__cty) / (dispy - 20);
	p_owner->__clx -= p_owner->__px * 10;
	p_owner->__crx += p_owner->__px * 10;
    p_owner->__cty -= p_owner->__py * 10;
    p_owner->__cby += p_owner->__py * 10;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "%s CALIBRATION : %d x %d", orientation[p_lcd->_rotation & 1], dispx, dispy);
    pifLog_Printf(LT_INFO, "px = %f, py = %f", p_owner->__px, p_owner->__py);
	pifLog_Printf(LT_INFO, "x = map(p.x, LEFT=%d, RT=%d, 0, %d)", p_owner->__clx, p_owner->__crx, dispx);
	pifLog_Printf(LT_INFO, "y = map(p.y, TOP=%d, BOT=%d, 0, %d)", p_owner->__cty, p_owner->__cby, dispy);
#endif

    int16_t x_range = p_owner->__clx - p_owner->__crx, y_range = p_owner->__cty - p_owner->__cby;
    if (abs(x_range) < 500 || abs(y_range) < 650) {
#ifndef PIF_NO_LOG
        pifLog_Printf(LT_INFO, "\n*** UNUSUAL CALIBRATION RANGES %d %d", x_range, y_range);
#endif
    	rtn = FALSE;
    	goto fail;
    }

    p_owner->_calibration = TRUE;

fail:
    (*p_lcd->_fn_draw_fill_rect)(p_lcd, 0, 0, dispx, dispy, BLACK);
    return rtn;
}
