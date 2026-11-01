// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rtc.h
 * @brief Common layer of the real-time clock chips: reads and writes the date and time of a chip and keeps
 *        pif_datetime in step with it.
 *
 * A chip driver, such as PifDs1302, puts a PifRtc at the beginning of its structure and gives pifRtc_Init() the
 * functions that read and write its registers. The application then works on the PifRtc alone, so another chip
 * takes its place without changing the application.
 *
 * The chips count whole seconds, so pifRtc_LoadDateTime() sets pif_datetime with millisecond 0. The sync task of
 * pifRtc_StartSync() moves pif_datetime forward when the chip has gone on to the next second, which also lines up
 * the second of pif_datetime with the tick of the chip, and back when pif_datetime runs 2 seconds or more ahead.
 */
#ifndef PIF_RTC_H
#define PIF_RTC_H


#include "core/pif_task_manager.h"


#define PIF_RTC_MAX_SYNC_PERIOD		4294	///< Longest sync period in seconds, as a period of the task in microseconds


struct StPifRtc;
typedef struct StPifRtc PifRtc;

/**
 * @brief Reads the date and time from the chip. millisecond is set to 0.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
typedef BOOL (*PifActRtcRead)(PifRtc *p_owner, PifDateTime *p_datetime);
/**
 * @brief Writes the date and time to the chip, which pifRtc_SetDateTime() has checked. millisecond is ignored.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
typedef BOOL (*PifActRtcWrite)(PifRtc *p_owner, const PifDateTime *p_datetime);

/**
 * @brief Reports that the sync task has corrected pif_datetime.
 * @param p_owner Pointer to the owner instance.
 * @param diff Chip minus pif_datetime before the correction, in seconds. 1 is the usual alignment to the tick of
 *             the chip; a larger value shows a drift or a pif_datetime set apart from the chip.
 * @param p_issuer Pointer given to pifRtc_StartSync().
 */
typedef void (*PifEvtRtcSync)(PifRtc *p_owner, int32_t diff, PifIssuerP p_issuer);

/**
 * @class StPifRtc
 * @brief Real-time clock chip.
 */
struct StPifRtc
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifTask *_p_task;				///< Sync task, or NULL

	// Private Member Variable
	PifIssuerP __p_issuer;

	// Private Action Function
	PifActRtcRead __act_read;
	PifActRtcWrite __act_write;

	// Private Event Function
	PifEvtRtcSync __evt_sync;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the common layer. Called by the chip driver.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param act_read Reads the chip.
 * @param act_write Writes the chip.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when a parameter is NULL.
 */
BOOL pifRtc_Init(PifRtc *p_owner, PifId id, PifActRtcRead act_read, PifActRtcWrite act_write);

/**
 * @brief Removes the sync task. Called by the chip driver.
 * @param p_owner Pointer to the owner instance.
 */
void pifRtc_Clear(PifRtc *p_owner);

/**
 * @brief Reads the date and time from the chip.
 * @param p_owner Pointer to the owner instance.
 * @param p_datetime Receives the date and time, with millisecond 0.
 * @return TRUE on success, FALSE with pif_error set by the chip driver otherwise.
 */
BOOL pifRtc_GetDateTime(PifRtc *p_owner, PifDateTime *p_datetime);

/**
 * @brief Writes the date and time to the chip. pif_datetime is not changed; call pif_SetDateTime() as well to
 *        change both.
 * @param p_owner Pointer to the owner instance.
 * @param p_datetime Date and time to write. year counts from 2000, and day must exist in its month. millisecond
 *                   is ignored.
 * @return TRUE on success. FALSE with E_INVALID_PARAM when a field is out of range, or with the error of the chip
 *         driver.
 */
BOOL pifRtc_SetDateTime(PifRtc *p_owner, const PifDateTime *p_datetime);

/**
 * @brief Reads the chip and sets pif_datetime to it with millisecond 0. Call it at startup, after pif_Init().
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRtc_LoadDateTime(PifRtc *p_owner);

/**
 * @brief Keeps pif_datetime in step with the chip on a task of its own: every period it reads the chip and moves
 *        pif_datetime forward when the chip is ahead, or back when pif_datetime is 2 seconds or more ahead.
 *        Fails with E_INVALID_STATE while that task is already running.
 * @param p_owner Pointer to the owner instance.
 * @param period1sec Period in seconds, 1 to PIF_RTC_MAX_SYNC_PERIOD.
 * @param evt_sync Gets each correction. May be NULL.
 * @param p_issuer Pointer handed to evt_sync.
 * @param name Name of the task. If NULL, "RTC" is used.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifRtc_StartSync(PifRtc *p_owner, uint16_t period1sec, PifEvtRtcSync evt_sync, PifIssuerP p_issuer,
	const char *name);

/**
 * @brief Removes the sync task.
 * @param p_owner Pointer to the owner instance.
 */
void pifRtc_StopSync(PifRtc *p_owner);

/**
 * @brief Checks the fields of a date and time as pif_SetDateTime() does. millisecond is not checked.
 * @param p_datetime Date and time to check.
 * @return TRUE when every field is in range and day exists in its month.
 */
BOOL pifRtc_CheckDateTime(const PifDateTime *p_datetime);

/**
 * @brief Gets the day of the week of a date.
 * @param p_datetime Date, which must be valid.
 * @return 0 for Sunday to 6 for Saturday.
 */
uint8_t pifRtc_GetWeekday(const PifDateTime *p_datetime);

/**
 * @brief Converts a date and time to seconds since 2000-01-01 00:00:00. millisecond is ignored.
 * @param p_datetime Date and time, which must be valid.
 * @return Seconds since 2000-01-01 00:00:00. It wraps around in 2136, which keeps the difference of two close
 *         values right.
 */
uint32_t pifRtc_ToSeconds(const PifDateTime *p_datetime);

/**
 * @brief Converts a binary coded decimal to binary.
 * @param bcd Two BCD digits.
 * @return 0 to 99.
 */
PIF_STATIC_INLINE uint8_t pifRtc_BcdToBin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }

/**
 * @brief Converts binary to a binary coded decimal.
 * @param bin 0 to 99.
 * @return Two BCD digits.
 */
PIF_STATIC_INLINE uint8_t pifRtc_BinToBcd(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

#ifdef __cplusplus
}
#endif


#endif  // PIF_RTC_H
