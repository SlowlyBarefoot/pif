#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "pifSensor.h"


#define PIF_SENSOR_SWITCH_FILTER_NONE		0
#define PIF_SENSOR_SWITCH_FILTER_COUNT		1
#define PIF_SENSOR_SWITCH_FILTER_CONTINUE	2


typedef enum _PIF_enSensorSwitchCsFlag
{
    SSCsF_enOff			= 0,

    SSCsF_enRawIdx		= 0,
	SSCsF_enFilterIdx	= 1,

	SSCsF_enRawBit		= 1,
    SSCsF_enFilterBit	= 2,
    SSCsF_enAllBit		= 3,

    SSCsF_enCount		= 2
} PIF_enSensorSwitchCsFlag;


struct _PIF_stSensorSwitch;
typedef struct _PIF_stSensorSwitch PIF_stSensorSwitch;

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

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct
{
	PIF_stSensorSwitch* p_owner;
    uint8_t flag;
    void* p_device[SSCsF_enCount];
    SWITCH state;
} PIF_SensorSwitchColSig;

#endif

/**
 * @class _PIF_stSensorSwitch
 * @brief
 */
struct _PIF_stSensorSwitch
{
	PifSensor stSensor;

	// Private Member Variable
    SWITCH __swState;

    uint8_t __ucFilterMethod;					// Default: PIF_SENSOR_SWITCH_FILTER_NONE
    PIF_stSensorSwitchFilter *__pstFilter;		// Default: NULL

#ifdef __PIF_COLLECT_SIGNAL__
    PIF_SensorSwitchColSig* __p_colsig;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

PifSensor* pifSensorSwitch_Create(PifId usPifId, SWITCH swInitState);
void pifSensorSwitch_Destroy(PifSensor** pp_sensor);

void pifSensorSwitch_InitialState(PifSensor *pstSensor);

BOOL pifSensorSwitch_AttachFilter(PifSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorSwitchFilter *pstFilter);
void pifSensorSwitch_DetachFilter(PifSensor *pstSensor);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorSwitch_SetCsFlagAll(PIF_enSensorSwitchCsFlag enFlag);
void pifSensorSwitch_ResetCsFlagAll(PIF_enSensorSwitchCsFlag enFlag);

void pifSensorSwitch_SetCsFlagEach(PifSensor *pstSensor, PIF_enSensorSwitchCsFlag enFlag);
void pifSensorSwitch_ResetCsFlagEach(PifSensor *pstSensor, PIF_enSensorSwitchCsFlag enFlag);

#endif

// Signal Function
void pifSensorSwitch_sigData(PifSensor *pstSensor, SWITCH swState);

// Task Function
PifTask *pifSensorSwitch_AttachTask(PifSensor *pstOwner, PifTaskMode enMode, uint16_t usPeriod, BOOL bStart);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
