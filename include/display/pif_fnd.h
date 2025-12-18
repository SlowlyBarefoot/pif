#ifndef PIF_FND_H
#define PIF_FND_H


#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"


#define PIF_FND_PERIOD_PER_DIGIT	25


typedef void (*PifActFndDisplay)(uint8_t segment, uint8_t digit);

/**
 * @class StPifFnd
 * @brief
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
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param digit_size
 * @param act_display
 * @return
 */
BOOL pifFnd_Init(PifFnd* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t digit_size, PifActFndDisplay act_display);

/**
 * @fn pifFnd_Clear
 * @brief
 * @param p_owner
 */
void pifFnd_Clear(PifFnd* p_owner);

/**
 * @fn pifFnd_SetUserChar
 * @brief
 * @param p_user_char
 * @param count
 */
void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count);

/**
 * @fn pifFnd_GetPeriodPerDigit
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifFnd_GetPeriodPerDigit(PifFnd* p_owner);

/**
 * @fn pifFnd_SetPeriodPerDigit
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifFnd_SetPeriodPerDigit(PifFnd* p_owner, uint16_t period1ms);

/**
 * @fn pifFnd_Start
 * @brief
 * @param p_owner
 */
void pifFnd_Start(PifFnd* p_owner);

/**
 * @fn pifFnd_Stop
 * @brief
 * @param p_owner
 */
void pifFnd_Stop(PifFnd* p_owner);

/**
 * @fn pifFnd_BlinkOn
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifFnd_BlinkOn(PifFnd* p_owner, uint16_t period1ms);

/**
 * @fn pifFnd_BlinkOff
 * @brief
 * @param p_owner
 */
void pifFnd_BlinkOff(PifFnd* p_owner);

/**
 * @fn pifFnd_ChangeBlinkPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifFnd_ChangeBlinkPeriod(PifFnd* p_owner, uint16_t period1ms);

/**
 * @fn pifFnd_SetFillZero
 * @brief
 * @param p_owner
 * @param fill_zero
 */
void pifFnd_SetFillZero(PifFnd* p_owner, BOOL fill_zero);

/**
 * @fn pifFnd_SetFloat
 * @brief
 * @param p_owner
 * @param value
 */
void pifFnd_SetFloat(PifFnd* p_owner, double value);

/**
 * @fn pifFnd_SetInterger
 * @brief
 * @param p_owner
 * @param value
 */
void pifFnd_SetInterger(PifFnd* p_owner, int32_t value);

/**
 * @fn pifFnd_SetString
 * @brief
 * @param p_owner
 * @param p_string
 */
void pifFnd_SetString(PifFnd* p_owner, char* p_string);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
