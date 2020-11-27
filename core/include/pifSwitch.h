#ifndef PIF_SWITCH_H
#define PIF_SWITCH_H


#include "pifTask.h"


#define PIF_SWITCH_FILTER_NONE		0
#define PIF_SWITCH_FILTER_COUNT		1
#define PIF_SWITCH_FILTER_CONTINUE	2


struct _PIF_stSwitchFilter;
typedef struct _PIF_stSwitchFilter PIF_stSwitchFilter;

typedef SWITCH (*PIF_actSwitchAcquire)(PIF_unDeviceCode unDeviceCode);
typedef void (*PIF_evtSwitchChange)(PIF_unDeviceCode unDeviceCode, SWITCH swState);
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
    PIF_unDeviceCode unDeviceCode;
    SWITCH swInitState;
    BOOL bStateReverse;							// Default: FALSE
    SWITCH swCurrState;							// Default: enInitState

	// Public Action Function
	PIF_actSwitchAcquire actAcquire;			// Default: NULL

	// Public Event Function
    PIF_evtSwitchChange evtChange;				// Default: NULL
} PIF_stSwitch;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSwitch_Init(uint8_t ucSize);
void pifSwitch_Exit();

PIF_stSwitch *pifSwitch_Add(PIF_unDeviceCode unDeviceCode, SWITCH swInitState);

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
