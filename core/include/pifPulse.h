#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pifTask.h"


#ifndef PIF_PWM_MAX_DUTY
#define PIF_PWM_MAX_DUTY		1000
#endif


typedef void (*PIF_actPulsePwm)(SWITCH swValue);

typedef void (*PIF_evtPulseFinish)(void *pvIssuer);


typedef enum _PIF_enPulseType
{
    PT_enOnce       = 0,
    PT_enRepeat		= 1,
	PT_enPwm		= 2
} PIF_enPulseType;

typedef enum _PIF_enPulseStep
{
    PS_enStop		= 0,
    PS_enRunning	= 1,
    PS_enRemove		= 2
} PIF_enPulseStep;


/**
 * @struct _PIF_stPulseItem
 * @brief 한 Pulse내에 항목을 관리하는 구조체
 */
typedef struct _PIF_stPulseItem
{
	// Public Member Variable

	// Read-onlyPublic Member Variable
    PIF_enPulseType _enType;
} PIF_stPulseItem;

/**
 * @struct _PIF_Pulse
 * @brief Pulse를 관리하기 위한 구조체
 */
typedef struct _PIF_stPulse
{
	// Public Member Variable

	// Read-onlyPublic Member Variable
	PIF_usId _usPifId;
	uint32_t _unPeriodUs;
} PIF_stPulse;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifPulse_Init(uint8_t ucSize);
void pifPulse_Exit();

PIF_stPulse *pifPulse_Add(PIF_usId usPifId, uint8_t ucSize, uint32_t unPeriodUs);

PIF_stPulseItem *pifPulse_AddItem(PIF_stPulse *pstOwner, PIF_enPulseType enType);
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_stPulseItem *pstItem);

BOOL pifPulse_StartItem(PIF_stPulseItem *pstItem, uint32_t unPulse);
void pifPulse_StopItem(PIF_stPulseItem *pstItem);

void pifPulse_SetPulse(PIF_stPulseItem *pstItem, uint32_t unPulse);
void pifPulse_SetPwmDuty(PIF_stPulseItem *pstItem, uint16_t usDuty);

PIF_enPulseStep pifPulse_GetStep(PIF_stPulseItem *pstItem);

uint32_t pifPulse_RemainItem(PIF_stPulseItem *pstItem);
uint32_t pifPulse_ElapsedItem(PIF_stPulseItem *pstItem);

// Signal Function
void pifPulse_sigTick(PIF_stPulse *pstOwner);

// Attach Action Function
void pifPulse_AttachAction(PIF_stPulseItem *pstItem, PIF_actPulsePwm actPwm);

// Attach Event Function
void pifPulse_AttachEvtFinish(PIF_stPulseItem *pstItem, PIF_evtPulseFinish evtFinish, void *pvIssuer);

// Task Function
void pifPulse_taskAll(PIF_stTask *pstTask);
void pifPulse_taskEach(PIF_stTask *pstTask);

#ifdef __PIF_DEBUG__

BOOL pifPulse_CheckItem(PIF_stPulse *pstOwner);

void pifPulse_PrintItemList(PIF_stPulse *pstOwner);
void pifPulse_PrintItemFree(PIF_stPulse *pstOwner);
void pifPulse_PrintItemAlloc(PIF_stPulse *pstOwner);

#endif  // __PIF_DEBUG__

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
