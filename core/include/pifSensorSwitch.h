#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "pifSensor.h"


#define PIF_SENSOR_SWITCH_FILTER_NONE		0
#define PIF_SENSOR_SWITCH_FILTER_COUNT		1
#define PIF_SENSOR_SWITCH_FILTER_CONTINUE	2


typedef enum EnPifSensorSwitchCsFlag
{
    SS_CSF_OFF			= 0,

    SS_CSF_RAW_IDX		= 0,
	SS_CSF_FILTER_IDX	= 1,

	SS_CSF_RAW_BIT		= 1,
    SS_CSF_FILTER_BIT	= 2,
    SS_CSF_ALL_BIT		= 3,

    SS_CSF_COUNT		= 2
} PifSensorSwitchCsFlag;


struct StPifSensorSwitch;
typedef struct StPifSensorSwitch PifSensorSwitch;

struct StPifSensorSwitchFilter;
typedef struct StPifSensorSwitchFilter PifSensorSwitchFilter;

typedef SWITCH (*PifEvtSensorSwitchFilter)(SWITCH state, PifSensorSwitchFilter* p_owner);


/**
 * @class StPifSensorSwitchFilter
 * @brief 
 */
struct StPifSensorSwitchFilter
{
    uint8_t size;
    uint8_t half;
    uint8_t count;
    uint32_t msb;
    uint32_t list;
    void* p_param;

	PifEvtSensorSwitchFilter evt_filter;
};

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct StPifSensorSwitchColSig
{
	PifSensorSwitch* p_owner;
    uint8_t flag;
    void* p_device[SS_CSF_COUNT];
    SWITCH state;
} PifSensorSwitchColSig;

#endif

/**
 * @class StPifSensorSwitch
 * @brief
 */
struct StPifSensorSwitch
{
	PifSensor stSensor;

	// Private Member Variable
    SWITCH __state;

    uint8_t __filter_method;				// Default: PIF_SENSOR_SWITCH_FILTER_NONE
    PifSensorSwitchFilter* __p_filter;		// Default: NULL

#ifdef __PIF_COLLECT_SIGNAL__
    PifSensorSwitchColSig* __p_colsig;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

PifSensor* pifSensorSwitch_Create(PifId id, SWITCH init_state);
void pifSensorSwitch_Destroy(PifSensor** pp_sensor);

void pifSensorSwitch_InitialState(PifSensor* p_parent);

BOOL pifSensorSwitch_AttachFilter(PifSensor* p_parent, uint8_t filter_method, uint8_t filter_size, PifSensorSwitchFilter* p_filter);
void pifSensorSwitch_DetachFilter(PifSensor* p_parent);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorSwitch_SetCsFlagAll(PifSensorSwitchCsFlag flag);
void pifSensorSwitch_ResetCsFlagAll(PifSensorSwitchCsFlag flag);

void pifSensorSwitch_SetCsFlagEach(PifSensor* p_parent, PifSensorSwitchCsFlag flag);
void pifSensorSwitch_ResetCsFlagEach(PifSensor* p_parent, PifSensorSwitchCsFlag flag);

#endif

// Signal Function
void pifSensorSwitch_sigData(PifSensor* p_parent, SWITCH state);

// Task Function
PifTask *pifSensorSwitch_AttachTask(PifSensor* p_parent, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
