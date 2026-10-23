// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_fnd.h
 * @brief Multiplexed seven-segment (FND) display showing numbers and text.
 *
 * Each scan step lights one digit through act_display. The steps run on a task of the display's own, or on an
 * external task of the application, given to pifFnd_Init(), that calls pifFnd_ProcessScan(). The latter suits
 * displays behind one bus, where one task can scan them all and order their writes. Several displays that
 * share the segment and digit lines can also share one scan as a group. A display shows a number or a string
 * through the font, or segments given as they are with pifFnd_SetSegments().
 */
#ifndef PIF_FND_H
#define PIF_FND_H


#include "core/pif_task_manager.h"


#define PIF_FND_PERIOD_PER_DIGIT	25	///< Time in ms to scan all digits once


/**
 * @brief Lights one digit with the given segments.
 *
 * id is the instance ID given to pifFnd_Init, so one callback can serve several displays.
 * Bits 0 to 6 of segment are the segments a to g and bit 7 is the decimal point. A segment of 0 turns the digit
 * off.
 */
typedef void (*PifActFndDisplay)(PifId id, uint8_t segment, uint8_t digit);

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
    PifTask* _p_task;				///< Task of the display's own, NULL with an external task

	// Private Member Variable
    PifTask* __p_ext_task;			///< External task given to pifFnd_Init(), owned by the application
    uint16_t __period_per_digit_1ms;	///< PIF_FND_PERIOD_PER_DIGIT
	struct {
		uint8_t blink		: 1;
		uint8_t fill_zero	: 1;
		uint8_t led			: 1;
		uint8_t run			: 1;
		uint8_t raw			: 1;	///< __p_string holds segments from pifFnd_SetSegments(), not characters
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
 *        Without p_task the display adds a task of its own, which runs at the frame period while the
 *        display runs. With p_task, that task of the application calls pifFnd_ProcessScan(), so that
 *        one task can scan several displays behind one bus and order their writes. The display neither
 *        changes, pauses nor removes it, and the frame period is only kept for the application.
 * @param p_owner Pointer to the FND instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param digit_size Number of physical digits on the display.
 * @param act_display Callback invoked to output segment/digit selection signals.
 * @param p_task External task that calls pifFnd_ProcessScan(), or NULL for a task of the display's own.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifFnd_Init(PifFnd* p_owner, PifId id, uint8_t digit_size, PifActFndDisplay act_display, PifTask* p_task);

/**
 * @brief Releases runtime resources allocated by the FND controller.
 *        A cleared member leaves its scan group. When a leader is cleared, each of its members
 *        goes back to scanning itself: on its external task, or with a task of its own again. A member
 *        that finds no free task slot stays dark until it joins another group or is initialized again.
 *        An external task is left to the application.
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
 *        The task of p_owner, if it has one of its own, is released, so a group of N displays takes
 *        one task slot, not N. A leader with an external task is scanned with its group by
 *        pifFnd_ProcessScan().
 * @param p_owner Display to hand over. It must not be in a scan group yet.
 * @param p_leader Display whose task scans the group. A member stands for its leader.
 * @return `TRUE` if the display joins the group, otherwise `FALSE`.
 */
BOOL pifFnd_ShareScan(PifFnd* p_owner, PifFnd* p_leader);

/**
 * @brief Outputs the next digit of the display, or of its group if it leads one, through act_display.
 *        It does nothing for a member, which its leader scans, or while no display of the group runs.
 *
 * The task of the display's own calls it. An external task given to pifFnd_Init() calls it every frame
 * period divided by the digits of the group, for instance for two displays on one I2C bus:
 *
 *     static uint32_t _taskFnd(PifTask* p_task)      // TM_PERIOD, period = frame / digits
 *     {
 *         for (int i = 0; i < FND_COUNT; i++) pifFnd_ProcessScan(&s_fnd[i]);
 *         // Write the digits that act_display left, one display after the other.
 *         return 0;
 *     }
 *
 * @param p_owner Pointer to an initialized FND instance with an external task.
 */
void pifFnd_ProcessScan(PifFnd* p_owner);

/**
 * @brief Registers user-defined segment patterns for custom characters.
 * @param p_user_char Pointer to custom character segment data.
 * @param count Number of custom characters in `p_user_char`.
 */
void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count);

/**
 * @brief Returns the time to scan all digits once (one frame). A member returns that of its leader.
 *        With an external task it is only kept, for the application that runs that task.
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

/**
 * @brief Displays segments as they are, one byte per digit, bit 0 to 6 for the segments a to g and bit 7 for
 *        the decimal point, as act_display takes them. Blink applies to them as to text. They stay until the
 *        next pifFnd_SetSegments(), pifFnd_SetString(), pifFnd_SetInteger() or pifFnd_SetFloat().
 * @param p_owner Pointer to an initialized FND instance.
 * @param p_segment Segments of every digit, _digit_size bytes. They are copied.
 */
void pifFnd_SetSegments(PifFnd* p_owner, const uint8_t* p_segment);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
