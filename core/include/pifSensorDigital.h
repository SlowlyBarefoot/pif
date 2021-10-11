#ifndef PIF_SENSOR_DIGITAL_H
#define PIF_SENSOR_DIGITAL_H


#include "pifSensor.h"


#define PIF_SENSOR_DIGITAL_FILTER_NONE		0
#define PIF_SENSOR_DIGITAL_FILTER_AVERAGE	1


typedef enum _PIF_enSensorDigitalEventType
{
	SDET_enNone			= 0,
	SDET_enPeriod		= 1,
	SDET_enThreshold1P	= 2,
	SDET_enThreshold2P	= 3
} PIF_enSensorDigitalEventType;

typedef enum _PIF_enSensorDigitalCsFlag
{
    SDCsF_enOff			= 0,

    SDCsF_enStateIdx	= 0,

	SDCsF_enStateBit	= 1,
    SDCsF_enAllBit		= 1,

    SDCsF_enCount		= 1
} PIF_enSensorDigitalCsFlag;


struct _PIF_stSensorDigital;
typedef struct _PIF_stSensorDigital PIF_stSensorDigital;

struct _PIF_stSensorDigitalFilter;
typedef struct _PIF_stSensorDigitalFilter PIF_stSensorDigitalFilter;

typedef void (*PIF_evtSensorDigitalPeriod)(PifId usPifId, uint16_t usLevel);
typedef uint16_t (*PIF_evtSensorDigitalFilter)(uint16_t usLevel, PIF_stSensorDigitalFilter *pstFilter);


/**
 * @class _PIF_stSensorDigitalFilter
 * @brief 
 */
struct _PIF_stSensorDigitalFilter
{
    uint8_t ucSize;
    uint8_t ucPos;
    uint16_t *apusBuffer;
    uint32_t unSum;
    void *pvParam;

	PIF_evtSensorDigitalFilter evtFilter;
};

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct
{
	PIF_stSensorDigital* p_owner;
    uint8_t flag;
    void* p_device[SDCsF_enCount];
} PIF_SensorDigitalColSig;

#endif

/**
 * @class _PIF_stSensorDigital
 * @brief
 */
struct _PIF_stSensorDigital
{
	PifSensor stSensor;

	// Private Member Variable
	PifPulse* __pstTimer;
    PIF_enSensorDigitalEventType __enEventType;
    union {
    	struct {
    		uint16_t usPeriod;
    	    PifPulseItem *pstTimerPeriod;
    	} stP;
		uint16_t usThreshold;
    	struct {
			uint16_t usThresholdLow;
			uint16_t usThresholdHigh;
    	} stT;
    } __ui;
    uint16_t __usCurrLevel;
    uint16_t __usPrevLevel;

    uint8_t __ucFilterMethod;					// Default: PIF_SENSOR_DIGITAL_FILTER_NONE
    PIF_stSensorDigitalFilter *__pstFilter;		// Default: NULL

#ifdef __PIF_COLLECT_SIGNAL__
    PIF_SensorDigitalColSig* __p_colsig;
#endif

	// Private Event Function
    PIF_evtSensorDigitalPeriod __evtPeriod;		// Default: NULL
};


#ifdef __cplusplus
extern "C" {
#endif

PifSensor* pifSensorDigital_Create(PifId usPifId, PifPulse* pstTimer);
void pifSensorDigital_Destroy(PifSensor** pp_sensor);

void pifSensorDigital_InitialState(PifSensor *pstSensor);

BOOL pifSensorDigital_AttachEvtPeriod(PifSensor *pstSensor, PIF_evtSensorDigitalPeriod evtPeriod);
BOOL pifSensorDigital_StartPeriod(PifSensor *pstSensor, uint16_t usPeriod);
void pifSensorDigital_StopPeriod(PifSensor *pstSensor);

void pifSensorDigital_SetEventThreshold1P(PifSensor *pstSensor, uint16_t usThreshold);
void pifSensorDigital_SetEventThreshold2P(PifSensor *pstSensor, uint16_t usThresholdLow, uint16_t usThresholdHigh);

BOOL pifSensorDigital_AttachFilter(PifSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorDigitalFilter *pstFilter, BOOL bInitFilter);
void pifSensorDigital_DetachFilter(PifSensor *pstSensor);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorDigital_SetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag);
void pifSensorDigital_ResetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag);

void pifSensorDigital_SetCsFlagEach(PifSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag);
void pifSensorDigital_ResetCsFlagEach(PifSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag);

#endif

// Signal Function
void pifSensorDigital_sigData(PifSensor *pstSensor, uint16_t usLevel);

// Task Function
PifTask *pifSensorDigital_AttachTask(PifSensor *pstOwner, PifTaskMode enMode, uint16_t usPeriod, BOOL bStart);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_DIGITAL_H
