#ifndef PIF_FND_H
#define PIF_FND_H


#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"


#define PIF_FND_PERIOD_PER_DIGIT	25


typedef void (*PifActFndDisplay)(uint8_t segment, uint8_t digit);

/**
 * @class StPifFnd
 * @brief Multiplexed seven-segment (FND) display controller context.
 */
typedef struct StPifFnd
{
	// Public Member Variable
	uint8_t sub_numeric_digits;

	// Read-only Member Variable
	PifId _id;
    uint8_t _count;
    uint8_t _digit_size;

	// Private Member Variable
    PifTask* __p_task;
    PifTimerManager* __p_timer_manager;
    uint16_t __period_per_digit_1ms;	// PIF_FND_PERIOD_PER_DIGIT
	struct {
		uint8_t blink		: 1;
		uint8_t fill_zero	: 1;
		uint8_t led			: 1;
	} __bt;
	uint8_t __digit_index;
	uint8_t __string_size;
    char* __p_string;
	PifTimer* __p_timer_blink;

	// Private Action Function
   	PifActFndDisplay __act_display;
} PifFnd;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifFnd_Init
 * @brief Initializes an FND controller and display output callback.
 * @param p_owner Pointer to the FND instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param p_timer_manager Timer manager used for refresh and blink timing.
 * @param digit_size Number of physical digits on the display.
 * @param act_display Callback invoked to output segment/digit selection signals.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifFnd_Init(PifFnd* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t digit_size, PifActFndDisplay act_display);

/**
 * @fn pifFnd_Clear
 * @brief Releases runtime resources allocated by the FND controller.
 * @param p_owner Pointer to the FND instance to clear.
 */
void pifFnd_Clear(PifFnd* p_owner);

/**
 * @fn pifFnd_SetUserChar
 * @brief Registers user-defined segment patterns for custom characters.
 * @param p_user_char Pointer to custom character segment data.
 * @param count Number of custom characters in `p_user_char`.
 */
void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count);

/**
 * @fn pifFnd_GetPeriodPerDigit
 * @brief Returns current refresh period used per digit in multiplex mode.
 * @param p_owner Pointer to an initialized FND instance.
 * @return Refresh period per digit in milliseconds.
 */
uint16_t pifFnd_GetPeriodPerDigit(PifFnd* p_owner);

/**
 * @fn pifFnd_SetPeriodPerDigit
 * @brief Sets refresh period used for each digit in multiplex scanning.
 * @param p_owner Pointer to an initialized FND instance.
 * @param period1ms Refresh period per digit in milliseconds.
 * @return `TRUE` if the period is accepted and applied, otherwise `FALSE`.
 */
BOOL pifFnd_SetPeriodPerDigit(PifFnd* p_owner, uint16_t period1ms);

/**
 * @fn pifFnd_Start
 * @brief Starts periodic multiplex refresh output.
 * @param p_owner Pointer to an initialized FND instance.
 */
void pifFnd_Start(PifFnd* p_owner);

/**
 * @fn pifFnd_Stop
 * @brief Stops periodic multiplex refresh output.
 * @param p_owner Pointer to an initialized FND instance.
 */
void pifFnd_Stop(PifFnd* p_owner);

/**
 * @fn pifFnd_BlinkOn
 * @brief Enables blink mode for the full display.
 * @param p_owner Pointer to an initialized FND instance.
 * @param period1ms Blink period in milliseconds.
 * @return `TRUE` if blink timer starts successfully, otherwise `FALSE`.
 */
BOOL pifFnd_BlinkOn(PifFnd* p_owner, uint16_t period1ms);

/**
 * @fn pifFnd_BlinkOff
 * @brief Disables blink mode and keeps display continuously active.
 * @param p_owner Pointer to an initialized FND instance.
 */
void pifFnd_BlinkOff(PifFnd* p_owner);

/**
 * @fn pifFnd_ChangeBlinkPeriod
 * @brief Changes blink timer period while blink mode is active.
 * @param p_owner Pointer to an initialized FND instance.
 * @param period1ms New blink period in milliseconds.
 * @return `TRUE` if period update succeeds, otherwise `FALSE`.
 */
BOOL pifFnd_ChangeBlinkPeriod(PifFnd* p_owner, uint16_t period1ms);

/**
 * @fn pifFnd_SetFillZero
 * @brief Configures leading-zero behavior when displaying numeric values.
 * @param p_owner Pointer to an initialized FND instance.
 * @param fill_zero `TRUE` to show leading zeros, `FALSE` to suppress them.
 */
void pifFnd_SetFillZero(PifFnd* p_owner, BOOL fill_zero);

/**
 * @fn pifFnd_SetFloat
 * @brief Converts and displays a floating-point value on the FND.
 * @param p_owner Pointer to an initialized FND instance.
 * @param value Floating-point value to render.
 */
void pifFnd_SetFloat(PifFnd* p_owner, double value);

/**
 * @fn pifFnd_SetInterger
 * @brief Converts and displays a signed integer value on the FND.
 * @param p_owner Pointer to an initialized FND instance.
 * @param value Integer value to render.
 */
void pifFnd_SetInterger(PifFnd* p_owner, int32_t value);

/**
 * @fn pifFnd_SetString
 * @brief Displays a text string using available segment character mappings.
 * @param p_owner Pointer to an initialized FND instance.
 * @param p_string Pointer to the string buffer to display.
 */
void pifFnd_SetString(PifFnd* p_owner, char* p_string);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
