#ifndef PIF_SWITCH_H
#define PIF_SWITCH_H


#include "pifTask.h"


#define PIF_SWITCH_FILTER_NONE		0
#define PIF_SWITCH_FILTER_COUNT		1
#define PIF_SWITCH_FILTER_CONTINUE	2


struct _PIF_stSwitchFilter;
typedef struct _PIF_stSwitchFilter PIF_stSwitchFilter;

typedef SWITCH (*PIF_actSwitchAcquire)(PIF_usId usPifId);
typedef void (*PIF_evtSwitchChange)(PIF_usId usPifId, SWITCH swState, void *pvIssuer);
typedef SWITCH (*PIF_evtSwitchFilter)(SWITCH swState, PIF_stSwitchFilter *pstOwner);


/**
 * @class _PIF_stSwitchFilter
 * @brief 
 */
struct _PIF_stSwitchFilter
{
    uint8_t ucSize;
    uint8_t ucHalf;
    uint8_t ucCount;
    uint32_t unMsb;
    uint32_t unList;
    void *pvParam;

	PIF_evtSwitchFilter evtFilter;
};

/**
 * @class _PIF_stSwitch
 * @brief
 */
typedef struct _PIF_stSwitch
{
	// Public Member Variable
    BOOL bStateReverse;						// Default: FALSE

    // Read-only Member Variable
    PIF_usId _usPifId;
    SWITCH _swInitState;
    SWITCH _swCurrState;					// Default: enInitState

	// Private Member Variable
    SWITCH __swPrevState;					// Default: enInitState
    void *__pvChangeIssuer;

    uint8_t __ucFilterMethod;				// Default: PIF_SWITCH_FILTER_NONE
    PIF_stSwitchFilter *__pstFilter;		// Default: NULL

	PIF_enTaskLoop __enTaskLoop;			// Default: TL_enAll

	// Public Action Function
	PIF_actSwitchAcquire __actAcquire;		// Default: NULL

	// Public Event Function
    PIF_evtSwitchChange __evtChange;		// Default: NULL
} PIF_stSwitch;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSwitch_Init(uint8_t ucSize);
void pifSwitch_Exit();

PIF_stSwitch *pifSwitch_Add(PIF_usId usPifId, SWITCH swInitState);

void pifSwitch_AttachAction(PIF_stSwitch *pstOwner, PIF_actSwitchAcquire actAcquire);

void pifSwitch_AttachEvtChange(PIF_stSwitch *pstOwner, PIF_evtSwitchChange evtChange, void *pvIssuer);
void pifSwitch_DetachEvtChange(PIF_stSwitch *pstOwner);

BOOL pifSwitch_AttachFilter(PIF_stSwitch *pstOwner, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSwitchFilter *pstFilter);
void pifSwitch_DetachFilter(PIF_stSwitch *pstOwner);

void pifSwitch_InitialState(PIF_stSwitch *pstOwner);

// Signal Function
void pifSwitch_sigData(PIF_stSwitch *pstOwner, SWITCH swState);

// Task Function
void pifSwitch_taskAll(PIF_stTask *pstTask);
void pifSwitch_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SWITCH_H
