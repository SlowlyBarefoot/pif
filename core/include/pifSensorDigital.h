#ifndef PIF_SENSOR_DIGITAL_H
#define PIF_SENSOR_DIGITAL_H


#include "pifSensor.h"


#define PIF_SENSOR_DIGITAL_FILTER_NONE		0
#define PIF_SENSOR_DIGITAL_FILTER_AVERAGE	1


typedef enum EnPifSensorDigitalEventType
{
	SDET_NONE			= 0,
	SDET_PERIOD			= 1,
	SDET_THRESHOLD_1P	= 2,
	SDET_THRESHOLD_2P	= 3
} PifSensorDigitalEventType;

typedef enum EnPifSensorDigitalCsFlag
{
    SD_CSF_OFF			= 0,

    SD_CSF_STATE_IDX	= 0,

	SD_CSF_STATE_BIT	= 1,
    SD_CSF_ALL_BIT		= 1,

    SD_CSF_COUNT		= 1
} PifSensorDigitalCsFlag;


struct StPifSensorDigital;
typedef struct StPifSensorDigital PifSensorDigital;

struct StPifSensorDigitalFilter;
typedef struct StPifSensorDigitalFilter PifSensorDigitalFilter;

typedef void (*PifEvtSensorDigitalPeriod)(PifId id, uint16_t level);
typedef uint16_t (*PifEvtSensorDigitalFilter)(uint16_t level, PifSensorDigitalFilter* p_filter);


/**
 * @class StPifSensorDigitalFilter
 * @brief 
 */
struct StPifSensorDigitalFilter
{
    uint8_t size;
    uint8_t pos;
    uint16_t* p_buffer;
    uint32_t sum;
    void* p_param;

	PifEvtSensorDigitalFilter evt_filter;
};

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct StPifSensorDigitalColSig
{
	PifSensorDigital* p_owner;
    uint8_t flag;
    void* p_device[SD_CSF_COUNT];
} PifSensorDigitalColSig;

#endif

/**
 * @class StPifSensorDigital
 * @brief
 */
struct StPifSensorDigital
{
	PifSensor parent;

	// Private Member Variable
	PifPulse* __p_timer;
    PifSensorDigitalEventType __event_type;
    union {
    	struct {
    		uint16_t time;
    	    PifPulseItem* p_timer;
    	} period;
		uint16_t threshold1p;
    	struct {
			uint16_t low;
			uint16_t high;
    	} threshold2p;
    } __ui;
    uint16_t __curr_level;
    uint16_t __prev_level;

    uint8_t __filter_method;					// Default: PIF_SENSOR_DIGITAL_FILTER_NONE
    PifSensorDigitalFilter* __p_filter;			// Default: NULL

#ifdef __PIF_COLLECT_SIGNAL__
    PifSensorDigitalColSig* __p_colsig;
#endif

	// Private Event Function
    PifEvtSensorDigitalPeriod __evt_period;		// Default: NULL
};


#ifdef __cplusplus
extern "C" {
#endif

PifSensor* pifSensorDigital_Create(PifId id, PifPulse* p_timer);
void pifSensorDigital_Destroy(PifSensor** pp_parent);

void pifSensorDigital_InitialState(PifSensor* p_parent);

BOOL pifSensorDigital_AttachEvtPeriod(PifSensor* p_parent, PifEvtSensorDigitalPeriod evt_period);
BOOL pifSensorDigital_StartPeriod(PifSensor* p_parent, uint16_t period);
void pifSensorDigital_StopPeriod(PifSensor* p_parent);

void pifSensorDigital_SetEventThreshold1P(PifSensor* p_parent, uint16_t threshold);
void pifSensorDigital_SetEventThreshold2P(PifSensor* p_parent, uint16_t threshold_low, uint16_t threshold_high);

BOOL pifSensorDigital_AttachFilter(PifSensor* p_parent, uint8_t filter_method, uint8_t filter_size,
		PifSensorDigitalFilter* p_filter, BOOL init_filter);
void pifSensorDigital_DetachFilter(PifSensor* p_parent);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorDigital_SetCsFlagAll(PifSensorDigitalCsFlag flag);
void pifSensorDigital_ResetCsFlagAll(PifSensorDigitalCsFlag flag);

void pifSensorDigital_SetCsFlagEach(PifSensor* p_parent, PifSensorDigitalCsFlag flag);
void pifSensorDigital_ResetCsFlagEach(PifSensor* p_parent, PifSensorDigitalCsFlag flag);

#endif

// Signal Function
void pifSensorDigital_sigData(PifSensor* p_parent, uint16_t level);

// Task Function
PifTask* pifSensorDigital_AttachTask(PifSensor* p_parent, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_DIGITAL_H
