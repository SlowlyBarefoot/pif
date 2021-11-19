#ifndef PIF_SENSOR_DIGITAL_H
#define PIF_SENSOR_DIGITAL_H


#include "pif_sensor.h"
#include "pif_timer.h"


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
	PifTimerManager* __p_timer_manager;
    PifSensorDigitalEventType __event_type;
    union {
    	struct {
    		uint16_t time;
    	    PifTimer* p_timer;
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

/**
 * @fn pifSensorDigital_Init
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @return 
 */
BOOL pifSensorDigital_Init(PifSensorDigital* p_owner, PifId id, PifTimerManager* p_timer_manager);

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
 * @fn pifSensorDigital_AttachEvtPeriod
 * @brief
 * @param p_owner
 * @param evt_period
 * @return
 */
BOOL pifSensorDigital_AttachEvtPeriod(PifSensorDigital* p_owner, PifEvtSensorDigitalPeriod evt_period);

/**
 * @fn pifSensorDigital_StartPeriod
 * @brief
 * @param p_owner
 * @param period
 * @return
 */
BOOL pifSensorDigital_StartPeriod(PifSensorDigital* p_owner, uint16_t period);

/**
 * @fn pifSensorDigital_StopPeriod
 * @brief
 * @param p_owner
 */
void pifSensorDigital_StopPeriod(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_SetEventThreshold1P
 * @brief
 * @param p_owner
 * @param threshold
 */
void pifSensorDigital_SetEventThreshold1P(PifSensorDigital* p_owner, uint16_t threshold);

/**
 * @fn pifSensorDigital_SetEventThreshold2P
 * @brief
 * @param p_owner
 * @param threshold_low
 * @param threshold_high
 */
void pifSensorDigital_SetEventThreshold2P(PifSensorDigital* p_owner, uint16_t threshold_low, uint16_t threshold_high);

/**
 * @fn pifSensorDigital_AttachFilter
 * @brief 
 * @param p_owner
 * @param filter_method
 * @param filter_size
 * @param p_filter
 * @param init_filter
 * @return 
 */
BOOL pifSensorDigital_AttachFilter(PifSensorDigital* p_owner, uint8_t filter_method, uint8_t filter_size,
		PifSensorDigitalFilter* p_filter, BOOL init_filter);

/**
 * @fn pifSensorDigital_DetachFilter
 * @brief
 * @param p_owner
 */
void pifSensorDigital_DetachFilter(PifSensorDigital* p_owner);

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSensorDigital_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSensorDigital_SetCsFlagAll(PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigital_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSensorDigital_ResetCsFlagAll(PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigital_SetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSensorDigital_SetCsFlagEach(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigital_ResetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSensorDigital_ResetCsFlagEach(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag);

#endif

/**
 * @fn pifSensorDigital_sigData
 * @brief 
 * @param p_owner
 * @param level
 */
void pifSensorDigital_sigData(PifSensorDigital* p_owner, uint16_t level);

/**
 * @fn pifSensorDigital_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifSensorDigital_AttachTask(PifSensorDigital* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_DIGITAL_H
