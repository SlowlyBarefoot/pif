#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pif_list.h"
#include "pifTask.h"


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


struct _PIF_stPulse;
typedef struct _PIF_stPulse PIF_stPulse;


/**
 * @struct _PIF_stPulseItem
 * @brief 한 Pulse내에 항목을 관리하는 구조체
 */
typedef struct _PIF_stPulseItem
{
	// Public Member Variable
    uint32_t unTarget;

	// Read-only Member Variable
    PIF_enPulseType _enType;
    PIF_enPulseStep _enStep;

	// Private Member Variable
	PIF_stPulse *__pstOwner;
    uint32_t __unCurrent;
    void *__pvFinishIssuer;
    uint32_t __unPwmDuty;
	BOOL __bEvent;

    // Private Action Function
    PIF_actPulsePwm __actPwm;

    // Private Event Function
    PIF_evtPulseFinish __evtFinish;
} PIF_stPulseItem;

/**
 * @struct _PIF_Pulse
 * @brief Pulse를 관리하기 위한 구조체
 */
struct _PIF_stPulse
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _usPifId;
	uint32_t _unPeriodUs;

	// Private Member Variable
    PIF_DList __items;
};


#ifdef __cplusplus
extern "C" {
#endif

PIF_stPulse *pifPulse_Create(PifId usPifId, uint32_t unPeriodUs);
void pifPulse_Destroy(PIF_stPulse **PpstOwner);

PIF_stPulseItem *pifPulse_AddItem(PIF_stPulse *pstOwner, PIF_enPulseType enType);
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_stPulseItem *pstItem);

BOOL pifPulse_StartItem(PIF_stPulseItem *pstItem, uint32_t unPulse);
void pifPulse_StopItem(PIF_stPulseItem *pstItem);

void pifPulse_SetPwmDuty(PIF_stPulseItem *pstItem, uint16_t usDuty);

uint32_t pifPulse_RemainItem(PIF_stPulseItem *pstItem);
uint32_t pifPulse_ElapsedItem(PIF_stPulseItem *pstItem);

// Signal Function
void pifPulse_sigTick(PIF_stPulse *pstOwner);

// Attach Action Function
void pifPulse_AttachAction(PIF_stPulseItem *pstItem, PIF_actPulsePwm actPwm);

// Attach Event Function
void pifPulse_AttachEvtFinish(PIF_stPulseItem *pstItem, PIF_evtPulseFinish evtFinish, void *pvIssuer);

// Task Function
PIF_stTask *pifPulse_AttachTask(PIF_stPulse *pstOwner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
