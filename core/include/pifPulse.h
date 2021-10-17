#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pif_list.h"
#include "pifTask.h"


typedef void (*PifActPulsePwm)(SWITCH value);

typedef void (*PifEvtPulseFinish)(void* p_issuer);


typedef enum EnPifPulseType
{
    PT_ONCE			= 0,
    PT_REPEAT		= 1,
	PT_PWM			= 2
} PifPulseType;

typedef enum EnPifPulseStep
{
    PS_STOP			= 0,
    PS_RUNNING		= 1,
    PS_REMOVE		= 2
} PifPulseStep;


struct StPifPulse;
typedef struct StPifPulse PifPulse;


/**
 * @struct StPifPulseItem
 * @brief 한 Pulse내에 항목을 관리하는 구조체
 */
typedef struct StPifPulseItem
{
	// Public Member Variable
    uint32_t target;

	// Read-only Member Variable
    PifPulseType _type;
    PifPulseStep _step;

	// Private Member Variable
	PifPulse* __p_owner;
    uint32_t __current;
    void* __p_finish_issuer;
    uint32_t __pwm_duty;
	BOOL __event;

    // Private Action Function
    PifActPulsePwm __act_pwm;

    // Private Event Function
    PifEvtPulseFinish __evt_finish;
} PifPulseItem;

/**
 * @struct _PIF_Pulse
 * @brief Pulse를 관리하기 위한 구조체
 */
struct StPifPulse
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	uint32_t _period1us;

	// Private Member Variable
    PifDList __items;
};


#ifdef __cplusplus
extern "C" {
#endif

PifPulse* pifPulse_Create(PifId id, uint32_t period1us);
void pifPulse_Destroy(PifPulse** pp_owner);

PifPulseItem* pifPulse_AddItem(PifPulse* p_owner, PifPulseType type);
void pifPulse_RemoveItem(PifPulseItem* p_item);

BOOL pifPulse_StartItem(PifPulseItem* p_item, uint32_t pulse);
void pifPulse_StopItem(PifPulseItem* p_item);

void pifPulse_SetPwmDuty(PifPulseItem* p_item, uint16_t duty);

uint32_t pifPulse_RemainItem(PifPulseItem* p_item);
uint32_t pifPulse_ElapsedItem(PifPulseItem* p_item);

// Signal Function
void pifPulse_sigTick(PifPulse* p_owner);

// Attach Action Function
void pifPulse_AttachAction(PifPulseItem* p_item, PifActPulsePwm act_pwm);

// Attach Event Function
void pifPulse_AttachEvtFinish(PifPulseItem* p_item, PifEvtPulseFinish evt_finish, void* p_issuer);

// Task Function
PifTask* pifPulse_AttachTask(PifPulse* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
