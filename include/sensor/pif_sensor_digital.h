#ifndef PIF_SENSOR_DIGITAL_H
#define PIF_SENSOR_DIGITAL_H


#include "core/pif_timer.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"


struct StPifSensorDigital;
typedef struct StPifSensorDigital PifSensorDigital;


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifSensorDigitalCsFlag
{
    SD_CSF_OFF			= 0,

    SD_CSF_STATE_IDX	= 0,

	SD_CSF_STATE_BIT	= 1,
    SD_CSF_ALL_BIT		= 1,

    SD_CSF_COUNT		= 1
} PifSensorDigitalCsFlag;

typedef struct StPifSensorDigitalColSig
{
	PifSensorDigital* p_owner;
    uint8_t flag;
    void* p_device[SD_CSF_COUNT];
} PifSensorDigitalColSig;

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifSensorDigital
 * @brief
 */
struct StPifSensorDigital
{
	// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Public Member Variable
    PifNoiseFilter* p_filter;

	// Private Member Variable
	uint16_t __low_threshold;
	uint16_t __high_threshold;
    uint16_t __curr_level;
    uint16_t __prev_level;

#ifdef PIF_COLLECT_SIGNAL
    PifSensorDigitalColSig* __p_colsig;
#endif

	// Private Event Function
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSensorDigital_Init
 * @brief 
 * @param p_owner
 * @param id
 * @param act_acquire
 * @return 
 */
BOOL pifSensorDigital_Init(PifSensorDigital* p_owner, PifId id, PifActSensorAcquire act_acquire);

/**
 * @fn pifSensorDigital_Clear
 * @brief 
 * @param p_owner
 */
void pifSensorDigital_Clear(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_InitialState
 * @brief
 * @param p_owner
 */
void pifSensorDigital_InitialState(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_SetThreshold
 * @brief
 * @param p_owner
 * @param low_threshold
 * @param high_threshold
 */
void pifSensorDigital_SetThreshold(PifSensorDigital* p_owner, uint16_t low_threshold, uint16_t high_threshold);

/**
 * @fn pifSensorDigital_sigData
 * @brief
 * @param p_owner
 * @param level
 */
void pifSensorDigital_sigData(PifSensorDigital* p_owner, uint16_t level);

/**
 * @fn pifSensorDigital_ProcessAcquire
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifSensorDigital_ProcessAcquire(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_AttachTaskAcquire
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifSensorDigital_AttachTaskAcquire(PifSensorDigital* p_owner, PifTaskMode mode, uint32_t period, BOOL start);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifSensorDigital_SetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSensorDigital_SetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigital_ResetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSensorDigital_ResetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigitalColSig_SetFlag
 * @brief
 * @param flag
 */
void pifSensorDigitalColSig_SetFlag(PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigitalColSig_ResetFlag
 * @brief
 * @param flag
 */
void pifSensorDigitalColSig_ResetFlag(PifSensorDigitalCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_DIGITAL_H
