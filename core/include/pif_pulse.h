#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pif_task.h"


#define PIF_PULSE_DATA_SIZE			4

#define PIF_PMM_PERIOD				0x01
#define PIF_PMM_LOW_WIDTH			0x02
#define PIF_PMM_HIGH_WIDTH			0x04
#define PIF_PMM_POSITION			0x08
#define PIF_PMM_RISING_COUNT		0x10
#define PIF_PMM_FALLING_COUNT		0x20


typedef enum EnPifPulseEdge
{
    PE_FALLING		= 0,
    PE_RISING		= 1
} PifPulseEdge;


typedef void (*PifEvtPulseEdge)(PifPulseEdge edge, void* p_issuer);


struct StPifPulse;
typedef struct StPifPulse PifPulse;


#ifdef __PIF_COLLECT_SIGNAL__

typedef enum EnPifGpioCsFlag
{
    PL_CSF_OFF			= 0,

	PL_CSF_STATE_IDX	= 0,

	PL_CSF_STATE_BIT	= 1,
	PL_CSF_ALL_BIT		= 1,

	PL_CSF_COUNT		= 1
} PifPulseCsFlag;

typedef struct
{
	PifPulse* p_owner;
    uint8_t flag;
    void* p_device[PL_CSF_COUNT];
} PifPulseColSig;

#endif

/**
 * @struct StPifPulseData
 * @brief
 */
typedef struct StPifPulsData
{
	uint32_t rising;
	uint32_t falling;
} PifPulseData;

/**
 * @struct StPifPulse
 * @brief Pulse를 관리하기 위한 구조체
 */
struct StPifPulse
{
	// Public Member Variable
	volatile uint32_t rising_count;
	volatile uint32_t falling_count;

	// Read-only Member Variable
	PifId _id;
	uint8_t	_measure_mode;		// PIF_PMM_XXX

	// Private Member Variable
	PifPulseData* __p_data;
	uint8_t __data_size;
	uint8_t __data_mask;
	uint8_t __ptr;
	uint8_t __count;
	struct {
		BOOL check		: 1;
		uint32_t min	: 31;
		uint32_t max	: 32;
	} __valid_range[4];
	void* __p_edge_issuer;
	uint8_t __channels;
	uint16_t __threshold_1us;

#ifdef __PIF_COLLECT_SIGNAL__
	PifPulseColSig* __p_colsig;
#endif

	// Private Event Function
	PifEvtPulseEdge __evt_edge;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPulse_Init
 * @brief Pulse를 초기화한다.
 * @param p_owner
 * @return 성공 여부를 반환한다.
 */
BOOL pifPulse_Init(PifPulse* p_owner, PifId id);

/**
 * @fn pifPulse_Clear
 * @brief Pulse내에 할당 메모리를 반환한다.
 * @param p_owner Pulse 자신
 */
void pifPulse_Clear(PifPulse* p_owner);

/**
 * @fn pifPulse_SetMeasureMode
 * @brief
 * @param p_owner
 * @param measure_mode
 */
void pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_ResetMeasureMode
 * @brief
 * @param p_owner
 * @param measure_mode
 */
void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_SetPositionModulation
 * @brief
 * @param p_owner
 * @param channels
 * @param threshold_1us
 * @return
 */
BOOL pifPulse_SetPositionModulation(PifPulse* p_owner, uint8_t channels, uint16_t threshold_1us);

/**
 * @fn pifPulse_SetValidRange
 * @brief
 * @param p_owner
 * @param measure_mode
 * @param min
 * @param max
 * @return
 */
BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint32_t min, uint32_t max);

/**
 * @fn pifPulse_ResetMeasureValue
 * @brief
 * @param p_owner
 */
void pifPulse_ResetMeasureValue(PifPulse* p_owner);

/**
 * @fn pifPulse_GetPeriod
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifPulse_GetPeriod(PifPulse* p_owner);

/**
 * @fn pifPulse_GetLowWidth
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifPulse_GetLowWidth(PifPulse* p_owner);

/**
 * @fn pifPulse_GetHighWidth
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifPulse_GetHighWidth(PifPulse* p_owner);

/**
 * @fn pifPulse_GetPositionModulation
 * @brief
 * @param p_owner
 * @param p_value
 * @return
 */
BOOL pifPulse_GetPositionModulation(PifPulse* p_owner, uint16_t* p_value);

/**
 * @fn pifPulse_sigEdgeTime
 * @brief
 * @param p_owner
 * @param edge
 * @param time_us
 */
void pifPulse_sigEdgeTime(PifPulse* p_owner, PifPulseEdge edge, uint32_t time_us);

/**
 * @fn pifPulse_sigEdge
 * @brief
 * @param p_owner
 * @param edge
 */
void pifPulse_sigEdge(PifPulse* p_owner, PifPulseEdge edge);

/**
 * @fn pifPulse_AttachEvtEdge
 * @brief
 * @param p_owner Pulse 포인터
 * @param evt_edge
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, void* p_issuer);

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifPulse_SetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifPulse_SetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag);

/**
 * @fn pifPulse_ResetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifPulse_ResetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag);

/**
 * @fn pifPulse_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifPulseColSig_SetFlag(PifPulseCsFlag flag);

/**
 * @fn pifPulse_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifPulseColSig_ResetFlag(PifPulseCsFlag flag);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
