// SPDX-License-Identifier: BSD-3-Clause
#include "rtc/pif_rtc.h"


static const uint16_t kDaysBeforeMonth[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };


/**
 * @fn _isLeapYear
 * @brief Checks whether a year has February 29.
 * @param year Year as of 2000.
 * @return TRUE for a leap year.
 */
static BOOL _isLeapYear(uint8_t year)
{
	uint16_t full = 2000 + year;

	return (full % 4 == 0 && full % 100 != 0) || full % 400 == 0;
}

/**
 * @fn _leapsBefore
 * @brief Counts the leap years from 1 to the year before full.
 * @param full Year with its century.
 * @return The count.
 */
static uint32_t _leapsBefore(uint16_t full)
{
	uint16_t last = full - 1;

	return last / 4 - last / 100 + last / 400;
}

/**
 * @fn _toDays
 * @brief Counts the days from 2000-01-01 to a date.
 * @param p_datetime Date, which must be valid.
 * @return The count.
 */
static uint32_t _toDays(const PifDateTime *p_datetime)
{
	uint32_t days;

	days = 365UL * p_datetime->year + _leapsBefore(2000 + p_datetime->year) - _leapsBefore(2000);
	days += kDaysBeforeMonth[p_datetime->month - 1] + p_datetime->day - 1;
	if (p_datetime->month > 2 && _isLeapYear(p_datetime->year)) days++;
	return days;
}

/**
 * @fn _doTask
 * @brief Reads the chip and corrects pif_datetime when it is out of step.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return 0 to keep the period.
 */
static uint32_t _doTask(PifTask *p_task)
{
	PifRtc *p_owner = (PifRtc *)p_task->_p_client;
	PifDateTime rtc, now;
	uint32_t state;
	int32_t diff;

	if (!(*p_owner->__act_read)(p_owner, &rtc)) return 0;

	state = pif_IrqDisable();
	now = *(PifDateTime *)&pif_datetime;
	pif_IrqRestore(state);

	// The chip ticks the second first when the system clock is slow, or when the last load set millisecond 0
	// before the tick of the chip. A system clock 1 second ahead is left alone, as it can be the same instant
	// read just after its own tick.
	diff = (int32_t)(pifRtc_ToSeconds(&rtc) - pifRtc_ToSeconds(&now));
	if (diff >= 1 || diff <= -2) {
		if (pif_SetDateTime(&rtc) && p_owner->__evt_sync) {
			(*p_owner->__evt_sync)(p_owner, diff, p_owner->__p_issuer);
		}
	}
	return 0;
}

BOOL pifRtc_Init(PifRtc *p_owner, PifId id, PifActRtcRead act_read, PifActRtcWrite act_write)
{
	if (!p_owner || !act_read || !act_write) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRtc));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->__act_read = act_read;
	p_owner->__act_write = act_write;
	return TRUE;
}

void pifRtc_Clear(PifRtc *p_owner)
{
	pifRtc_StopSync(p_owner);
}

BOOL pifRtc_GetDateTime(PifRtc *p_owner, PifDateTime *p_datetime)
{
	if (!p_owner || !p_datetime) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	return (*p_owner->__act_read)(p_owner, p_datetime);
}

BOOL pifRtc_SetDateTime(PifRtc *p_owner, const PifDateTime *p_datetime)
{
	if (!p_owner || !pifRtc_CheckDateTime(p_datetime)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	return (*p_owner->__act_write)(p_owner, p_datetime);
}

BOOL pifRtc_LoadDateTime(PifRtc *p_owner)
{
	PifDateTime datetime;

	if (!pifRtc_GetDateTime(p_owner, &datetime)) return FALSE;
	return pif_SetDateTime(&datetime);
}

BOOL pifRtc_StartSync(PifRtc *p_owner, uint16_t period1sec, PifEvtRtcSync evt_sync, PifIssuerP p_issuer,
	const char *name)
{
	if (!p_owner || !period1sec || period1sec > PIF_RTC_MAX_SYNC_PERIOD) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_p_task) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1sec * 1000000UL, _doTask, p_owner, TRUE);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = name ? name : "RTC";

	p_owner->__evt_sync = evt_sync;
	p_owner->__p_issuer = p_issuer;
	return TRUE;
}

void pifRtc_StopSync(PifRtc *p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifRtc_CheckDateTime(const PifDateTime *p_datetime)
{
	uint8_t days;

	if (!p_datetime || p_datetime->month < 1 || p_datetime->month > 12 || p_datetime->day < 1 ||
			p_datetime->hour >= 24 || p_datetime->minute >= 60 || p_datetime->second >= 60) return FALSE;

	if (p_datetime->month == 12) {
		days = 31;
	}
	else {
		days = kDaysBeforeMonth[p_datetime->month] - kDaysBeforeMonth[p_datetime->month - 1];
		if (p_datetime->month == 2 && _isLeapYear(p_datetime->year)) days++;
	}
	return p_datetime->day <= days;
}

uint8_t pifRtc_GetWeekday(const PifDateTime *p_datetime)
{
	// 2000-01-01 was a Saturday.
	return (_toDays(p_datetime) + 6) % 7;
}

uint32_t pifRtc_ToSeconds(const PifDateTime *p_datetime)
{
	return ((_toDays(p_datetime) * 24 + p_datetime->hour) * 60 + p_datetime->minute) * 60 + p_datetime->second;
}
