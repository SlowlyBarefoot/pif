#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "filter/pif_noise_filter.h"
#include "core/pif_task.h"
#include "sensor/pif_sensor.h"


struct StPifSensorSwitch;
typedef struct StPifSensorSwitch PifSensorSwitch;

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

#endif	// __PIF_COLLECT_SIGNAL__

/**
 * @class StPifSensorSwitch
 * @brief
 */
struct StPifSensorSwitch
{
	PifSensor parent;

	// Public Member Variable
    PifNoiseFilter* p_filter;

	// Private Member Variable
    SWITCH __state;

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
 * @param act_acquire
 * @return 
 */
BOOL pifSensorSwitch_Init(PifSensorSwitch* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire);

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
 * @fn pifSensorSwitch_sigData
 * @brief
 * @param p_owner
 * @param swState
 */
void pifSensorSwitch_sigData(PifSensorSwitch* p_owner, SWITCH state);

/**
 * @fn pifSensorSwitch_ProcessAcquire
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifSensorSwitch_ProcessAcquire(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_AttachTaskAcquire
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask *pifSensorSwitch_AttachTaskAcquire(PifSensorSwitch* p_owner, PifTaskMode mode, uint16_t period, BOOL start);


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

#endif	// __PIF_COLLECT_SIGNAL__

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
