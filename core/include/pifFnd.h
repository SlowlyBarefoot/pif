#ifndef PIF_FND_H
#define PIF_FND_H


#include "pifPulse.h"


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
    PifPulse* __p_timer;
	struct {
		uint8_t run			: 1;
		uint8_t blink		: 1;
		uint8_t fill_zero	: 1;
		uint8_t led			: 1;
	} __bt;
	uint8_t __digit_index;
	uint8_t __string_size;
    char* __p_string;
	PifPulseItem* __p_timer_blink;

	// Private Action Function
   	PifActFndDisplay __act_display;
} PifFnd;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifFnd_Create
 * @brief
 * @param id
 * @param p_timer
 * @param digit_size
 * @param act_display
 * @return
 */
PifFnd* pifFnd_Create(PifId id, PifPulse* p_timer, uint8_t digit_size, PifActFndDisplay act_display);

/**
 * @fn pifFnd_Destroy
 * @brief
 * @param pp_owner
 */
void pifFnd_Destroy(PifFnd** pp_owner);

/**
 * @fn pifFnd_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer
 * @param digit_size
 * @param act_display
 * @return
 */
BOOL pifFnd_Init(PifFnd* p_owner, PifId id, PifPulse* p_timer, uint8_t digit_size, PifActFndDisplay act_display);

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

/**
 * @fn pifFnd_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifFnd_AttachTask(PifFnd* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
