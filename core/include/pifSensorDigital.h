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


struct _PIF_stSensorDigitalFilter;
typedef struct _PIF_stSensorDigitalFilter PIF_stSensorDigitalFilter;

typedef void (*PIF_evtSensorDigitalPeriod)(PIF_usId usPifId, uint16_t usLevel);
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

/**
 * @class _PIF_stSensorDigital
 * @brief
 */
typedef struct _PIF_stSensorDigital
{
	PIF_stSensor stSensor;

	// Private Member Variable
	uint8_t __ucIndex;
    PIF_enSensorDigitalEventType __enEventType;
    union {
    	struct {
    		uint16_t usPeriod;
    	    PIF_stPulseItem *pstTimerPeriod;
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
    uint8_t __ucCsFlag;
    int8_t __cCsIndex[SDCsF_enCount];
#endif

	// Private Event Function
    PIF_evtSensorDigitalPeriod __evtPeriod;		// Default: NULL
} PIF_stSensorDigital;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSensorDigital_Init(uint8_t ucSize, PIF_stPulse *pstTimer);
void pifSensorDigital_Exit();

PIF_stSensor *pifSensorDigital_Add(PIF_usId usPifId);

void pifSensorDigital_InitialState(PIF_stSensor *pstSensor);

BOOL pifSensorDigital_AttachEvtPeriod(PIF_stSensor *pstSensor, PIF_evtSensorDigitalPeriod evtPeriod);
BOOL pifSensorDigital_StartPeriod(PIF_stSensor *pstSensor, uint16_t usPeriod);
void pifSensorDigital_StopPeriod(PIF_stSensor *pstSensor);

void pifSensorDigital_SetEventThreshold1P(PIF_stSensor *pstSensor, uint16_t usThreshold);
void pifSensorDigital_SetEventThreshold2P(PIF_stSensor *pstSensor, uint16_t usThresholdLow, uint16_t usThresholdHigh);

BOOL pifSensorDigital_AttachFilter(PIF_stSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorDigitalFilter *pstFilter, BOOL bInitFilter);
void pifSensorDigital_DetachFilter(PIF_stSensor *pstSensor);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorDigital_SetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag);
void pifSensorDigital_ResetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag);

void pifSensorDigital_SetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag);
void pifSensorDigital_ResetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag);

#endif

// Signal Function
void pifSensorDigital_sigData(PIF_stSensor *pstSensor, uint16_t usLevel);

// Task Function
uint16_t pifSensorDigital_Task(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_DIGITAL_H
