#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "core/pif_task.h"


#define PIF_PULSE_DATA_SIZE			4
#define PIF_PULSE_DATA_MASK			(PIF_PULSE_DATA_SIZE - 1)

#define PIF_PMM_PERIOD				1
#define PIF_PMM_COUNT				2
#define PIF_PMM_LOW_WIDTH			4
#define PIF_PMM_HIGH_WIDTH			8


typedef void (*PifEvtPulseEdge)(PifPulseState state, PifIssuerP p_issuer);


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

#endif	// __PIF_COLLECT_SIGNAL__

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
	volatile uint32_t falling_count;

	// Read-only Member Variable
	PifId _id;
	uint8_t	_measure_mode;		// PIF_PMM_XXX

	// Private Member Variable
	PifPulseData __data[PIF_PULSE_DATA_SIZE];
	uint8_t __ptr, __last_ptr;
	uint8_t __count;
	struct {
		unsigned int check	: 1;
		unsigned int min	: 15;
		uint16_t max;
	} __valid_range[3];
	PifIssuerP __p_issuer;

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
 * @return
 */
BOOL pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_ResetMeasureMode
 * @brief
 * @param p_owner
 * @param measure_mode
 */
void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

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
 * @fn pifPulse_sigEdge
 * @param p_owner
 * @param state	PS_RISING_EDGE, PS_FALLING_EDGE
 * @param time_us
 * @return
 */
BOOL pifPulse_sigEdge(PifPulse* p_owner, PifPulseState state, uint32_t time_us);

/**
 * @fn pifPulse_AttachEvtEdge
 * @brief
 * @param p_owner Pulse 포인터
 * @param evt_edge
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, PifIssuerP p_issuer);

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

#endif	// __PIF_COLLECT_SIGNAL__

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
