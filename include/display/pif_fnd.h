// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_fnd.h
 * @brief Multiplexed seven-segment (FND) display showing numbers and text.
 *
 * A task lights one digit at a time through act_display. Several displays can share one scanning task as a
 * group.
 */
#ifndef PIF_FND_H
#define PIF_FND_H


#include "core/pif_task_manager.h"


#define PIF_FND_PERIOD_PER_DIGIT	25	///< Time in ms to scan all digits once


/**
 * @brief Lights one digit with the given segments.
 *
 * Bits 0 to 6 of segment are the segments a to g and bit 7 is the decimal point. A segment of 0 turns the digit
 * off.
 */
typedef void (*PifActFndDisplay)(uint8_t segment, uint8_t digit);

/**
 * @class StPifFnd
 * @brief Multiplexed seven-segment (FND) display controller context.
 */
typedef struct StPifFnd
{
	// Public Member Variable
	uint8_t sub_numeric_digits;	///< Digits after the decimal point when a float is shown

	// Read-only Member Variable
	PifId _id;							///< Instance ID
    uint8_t _count;						///< Reserved, not used
    uint8_t _digit_size;				///< Number of digits

	// Private Member Variable
    PifTask* __p_task;
    uint16_t __period_per_digit_1ms;	///< PIF_FND_PERIOD_PER_DIGIT
	struct {
		uint8_t blink		: 1;
		uint8_t fill_zero	: 1;
		uint8_t led			: 1;
		uint8_t run			: 1;
	} __bt;
	uint8_t __digit_index;			///< Digit of __p_scan to output next
	uint8_t __string_size;
    char* __p_string;
	uint16_t __blink_period_1ms;
	uint32_t __blink_time_us;		///< Time of the last blink toggle

	/// Scan group. Every instance scans itself until pifFnd_ShareScan() hands it to a leader.
	struct StPifFnd* __p_leader;	///< Instance whose task scans this one, NULL for a leader
	struct StPifFnd* __p_next;		///< Next instance in the scan of the leader
	struct StPifFnd* __p_scan;		///< Leader only: instance whose digit is output next

	// Private Action Function
   	PifActFndDisplay __act_display;
} PifFnd;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an FND controller and display output callback.
 * @param p_owner Pointer to the FND instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param digit_size Number of physical digits on the display.
 * @param act_display Callback invoked to output segment/digit selection signals.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifFnd_Init(PifFnd* p_owner, PifId id, uint8_t digit_size, PifActFndDisplay act_display);

/**
 * @brief Releases runtime resources allocated by the FND controller.
 *        A cleared member leaves its scan group. When a leader is cleared, each of its members
 *        gets a task of its own again and goes back to scanning itself. A member that finds no
 *        free task slot stays dark until it joins another group or is initialized again.
 * @param p_owner Pointer to the FND instance to clear.
 */
void pifFnd_Clear(PifFnd* p_owner);

/**
 * @brief Hands the scan of a display to the task of another one, for displays that share
 *        the segment and digit lines. The leader's task then outputs every digit of the group
 *        in turn, so only one digit of the shared lines is on at a time and each gets the same time.
 *        Each display keeps its own value, settings, blink, start and stop, and its digits are
 *        still output through its own callback.
 *        The frame period of the leader becomes the time to scan all digits of the group once.
 *        The task of p_owner is released, so a group of N displays takes one task slot, not N.
 * @param p_owner Display to hand over. It must not be in a scan group yet.
 * @param p_leader Display whose task scans the group. A member stands for its leader.
 * @return `TRUE` if the display joins the group, otherwise `FALSE`.
 */
BOOL pifFnd_ShareScan(PifFnd* p_owner, PifFnd* p_leader);

/**
 * @brief Registers user-defined segment patterns for custom characters.
 * @param p_user_char Pointer to custom character segment data.
 * @param count Number of custom characters in `p_user_char`.
 */
void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count);

/**
 * @brief Returns the time to scan all digits once (one frame). A member returns that of its leader.
 * @param p_owner Pointer to an initialized FND instance.
 * @return Frame period in milliseconds.
 */
uint16_t pifFnd_GetPeriodPerDigit(PifFnd* p_owner);

/**
 * @brief Sets the time to scan all digits once (one frame). Each digit is output every
 *        period1ms / digit count. For a member it sets the period of its leader's group.
 * @param p_owner Pointer to an initialized FND instance.
 * @param period1ms Frame period in milliseconds.
 * @return `TRUE` if the period is accepted and applied, otherwise `FALSE`.
 */
BOOL pifFnd_SetPeriodPerDigit(PifFnd* p_owner, uint16_t period1ms);

/**
 * @brief Starts periodic multiplex refresh output. Blink advances only while it runs.
 * @param p_owner Pointer to an initialized FND instance.
 */
void pifFnd_Start(PifFnd* p_owner);

/**
 * @brief Turns the digits off and stops periodic multiplex refresh output. Blink is stopped too.
 *        In a scan group the other displays go on.
 * @param p_owner Pointer to an initialized FND instance.
 */
void pifFnd_Stop(PifFnd* p_owner);

/**
 * @brief Enables blink mode for the full display.
 * @param p_owner Pointer to an initialized FND instance.
 * @param period1ms Blink period in milliseconds.
 * @return `TRUE` if blink starts, otherwise `FALSE`.
 */
BOOL pifFnd_BlinkOn(PifFnd* p_owner, uint16_t period1ms);

/**
 * @brief Disables blink mode and keeps display continuously active.
 * @param p_owner Pointer to an initialized FND instance.
 */
void pifFnd_BlinkOff(PifFnd* p_owner);

/**
 * @brief Changes blink period while blink mode is active.
 * @param p_owner Pointer to an initialized FND instance.
 * @param period1ms New blink period in milliseconds.
 * @return `TRUE` if period update succeeds, otherwise `FALSE`.
 */
BOOL pifFnd_ChangeBlinkPeriod(PifFnd* p_owner, uint16_t period1ms);

/**
 * @brief Configures leading-zero behavior when displaying numeric values.
 * @param p_owner Pointer to an initialized FND instance.
 * @param fill_zero `TRUE` to show leading zeros, `FALSE` to suppress them.
 */
void pifFnd_SetFillZero(PifFnd* p_owner, BOOL fill_zero);

/**
 * @brief Converts and displays a floating-point value on the FND.
 * @param p_owner Pointer to an initialized FND instance.
 * @param value Floating-point value to render.
 */
void pifFnd_SetFloat(PifFnd* p_owner, double value);

/**
 * @brief Converts and displays a signed integer value on the FND.
 * @param p_owner Pointer to an initialized FND instance.
 * @param value Integer value to render.
 */
void pifFnd_SetInteger(PifFnd* p_owner, int32_t value);

/**
 * @brief Displays a text string using available segment character mappings.
 * @param p_owner Pointer to an initialized FND instance.
 * @param p_string Pointer to the string buffer to display.
 */
void pifFnd_SetString(PifFnd* p_owner, char* p_string);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
