#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "pif_sensor.h"


#define PIF_SENSOR_SWITCH_FILTER_NONE		0
#define PIF_SENSOR_SWITCH_FILTER_COUNT		1
#define PIF_SENSOR_SWITCH_FILTER_CONTINUE	2


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
	PifSensor parent;

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

/**
 * @fn pifSensorSwitch_Init
 * @brief 
 * @param p_owner
 * @param id
 * @param init_state
 * @return 
 */
BOOL pifSensorSwitch_Init(PifSensorSwitch* p_owner, PifId id, SWITCH init_state);

/**
 * @fn pifSensorSwitch_Clear
 * @brief
 * @param p_owner
 */
void pifSensorSwitch_Clear(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_InitialState
 * @brief
 * @param p_owner
 */
void pifSensorSwitch_InitialState(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_AttachFilter
 * @brief
 * @param p_owner
 * @param filter_method
 * @param filter_size
 * @param p_filter
 * @return
 */
BOOL pifSensorSwitch_AttachFilter(PifSensorSwitch* p_owner, uint8_t filter_method, uint8_t filter_size, PifSensorSwitchFilter* p_filter);

/**
 * @fn pifSensorSwitch_DetachFilter
 * @brief
 * @param p_owner
 */
void pifSensorSwitch_DetachFilter(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_sigData
 * @brief
 * @param p_owner
 * @param swState
 */
void pifSensorSwitch_sigData(PifSensorSwitch* p_owner, SWITCH state);

/**
 * @fn pifSensorSwitch_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask *pifSensorSwitch_AttachTask(PifSensorSwitch* p_owner, PifTaskMode mode, uint16_t period, BOOL start);


#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSensorSwitch_SetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSensorSwitch_SetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag);

/**
 * @fn pifSensorSwitch_ResetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSensorSwitch_ResetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag);

/**
 * @fn pifSensorSwitchColSig_SetFlag
 * @brief
 * @param flag
 */
void pifSensorSwitchColSig_SetFlag(PifSensorSwitchCsFlag flag);

/**
 * @fn pifSensorSwitchColSig_ResetFlag
 * @brief
 * @param flag
 */
void pifSensorSwitchColSig_ResetFlag(PifSensorSwitchCsFlag flag);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
