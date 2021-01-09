#ifndef PIF_SENSOR_H
#define PIF_SENSOR_H


#include "pifPulse.h"
#include "pifTask.h"


#define PIF_SENSOR_FILTER_NONE		0
#define PIF_SENSOR_FILTER_AVERAGE	1


typedef enum _PIF_enSensorEventType
{
	SET_enNone			= 0,
	SET_enPeriod		= 1,
	SET_enThreshold1P	= 2,
	SET_enThreshold2P	= 3
} PIF_enSensorEventType;


struct _PIF_stSensorFilter;
typedef struct _PIF_stSensorFilter PIF_stSensorFilter;

typedef void (*PIF_evtSensorPeriod)(PIF_usId usPifId, uint16_t usLevel);
typedef void (*PIF_evtSensorChange)(PIF_usId usPifId, SWITCH swState);
typedef uint16_t (*PIF_evtSensorFilter)(uint16_t usLevel, PIF_stSensorFilter *pstOwner);

/**
 * @class _PIF_stSensorFilter
 * @brief 
 */
struct _PIF_stSensorFilter
{
    uint8_t ucSize;
    uint8_t ucPos;
    uint16_t *apusBuffer;
    uint32_t unSum;
    void *pvParam;

	PIF_evtSensorFilter evtFilter;
};

/**
 * @class _PIF_stSensor
 * @brief
 */
typedef struct _PIF_stSensor
{
	// Public Member Variable
    PIF_usId usPifId;
} PIF_stSensor;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSensor_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifSensor_Exit();

PIF_stSensor *pifSensor_Add(PIF_usId usPifId);

BOOL pifSensor_SetEventPeriod(PIF_stSensor *pstOwner, PIF_evtSensorPeriod evtPeriod);
BOOL pifSensor_StartPeriod(PIF_stSensor *pstOwner, uint16_t usPeriod);
void pifSensor_StopPeriod(PIF_stSensor *pstOwner);

void pifSensor_SetEventThreshold1P(PIF_stSensor *pstOwner, uint16_t usThreshold, PIF_evtSensorChange evtChange);

void pifSensor_SetEventThreshold2P(PIF_stSensor *pstOwner, uint16_t usThresholdLow, uint16_t usThresholdHigh, PIF_evtSensorChange evtChange);

BOOL pifSensor_AttachFilter(PIF_stSensor *pstOwner, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorFilter *pstFilter, BOOL bInitFilter);
void pifSensor_DetachFilter(PIF_stSensor *pstOwner);

// Signal Function
void pifSensor_sigData(PIF_stSensor *pstOwner, uint16_t usLevel);

// Task Function
void pifSensor_taskAll(PIF_stTask *pstTask);
void pifSensor_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_H
