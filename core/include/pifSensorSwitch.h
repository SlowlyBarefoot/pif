#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "pifSensor.h"


#define PIF_SENSOR_SWITCH_FILTER_NONE		0
#define PIF_SENSOR_SWITCH_FILTER_COUNT		1
#define PIF_SENSOR_SWITCH_FILTER_CONTINUE	2


struct _PIF_stSensorSwitchFilter;
typedef struct _PIF_stSensorSwitchFilter PIF_stSensorSwitchFilter;

typedef SWITCH (*PIF_evtSensorSwitchFilter)(SWITCH swState, PIF_stSensorSwitchFilter *pstOwner);


/**
 * @class _PIF_stSensorSwitchFilter
 * @brief 
 */
struct _PIF_stSensorSwitchFilter
{
    uint8_t ucSize;
    uint8_t ucHalf;
    uint8_t ucCount;
    uint32_t unMsb;
    uint32_t unList;
    void *pvParam;

	PIF_evtSensorSwitchFilter evtFilter;
};

/**
 * @class _PIF_stSensorSwitch
 * @brief
 */
typedef struct _PIF_stSensorSwitch
{
	PIF_stSensor stSensor;

	// Private Member Variable
    SWITCH __swState;

    uint8_t __ucFilterMethod;					// Default: PIF_SENSOR_SWITCH_FILTER_NONE
    PIF_stSensorSwitchFilter *__pstFilter;		// Default: NULL
} PIF_stSensorSwitch;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSensorSwitch_Init(uint8_t ucSize);
void pifSensorSwitch_Exit();

PIF_stSensor *pifSensorSwitch_Add(PIF_usId usPifId, SWITCH swInitState);

void pifSensorSwitch_InitialState(PIF_stSensor *pstSensor);

BOOL pifSensorSwitch_AttachFilter(PIF_stSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorSwitchFilter *pstFilter);
void pifSensorSwitch_DetachFilter(PIF_stSensor *pstSensor);

// Signal Function
void pifSensorSwitch_sigData(PIF_stSensor *pstSensor, SWITCH swState);

// Task Function
void pifSensorSwitch_taskAll(PIF_stTask *pstTask);
void pifSensorSwitch_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
