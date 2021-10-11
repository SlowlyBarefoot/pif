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
	} __bt;
    uint16_t __control_period1ms;
	uint16_t __pretime1ms;
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

PifFnd* pifFnd_Create(PifId id, PifPulse* p_timer, uint8_t digit_size, PifActFndDisplay act_display);
void pifFnd_Destroy(PifFnd** pp_owner);

void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count);

BOOL pifFnd_SetControlPeriod(PifFnd* p_owner, uint16_t period1ms);

void pifFnd_Start(PifFnd* p_owner);
void pifFnd_Stop(PifFnd* p_owner);

BOOL pifFnd_BlinkOn(PifFnd* p_owner, uint16_t period1ms);
void pifFnd_BlinkOff(PifFnd* p_owner);
BOOL pifFnd_ChangeBlinkPeriod(PifFnd* p_owner, uint16_t period1ms);

void pifFnd_SetFillZero(PifFnd* p_owner, BOOL fill_zero);
void pifFnd_SetFloat(PifFnd* p_owner, double value);
void pifFnd_SetInterger(PifFnd* p_owner, int32_t value);
void pifFnd_SetString(PifFnd* p_owner, char* p_string);

// Task Function
PifTask* pifFnd_AttachTask(PifFnd* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
