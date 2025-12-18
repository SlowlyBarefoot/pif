#ifndef PIF_RC_PWM_H
#define PIF_RC_PWM_H


#include "rc/pif_rc.h"


#define PIF_RC_PWM_DATA_SIZE			4
#define PIF_RC_PWM_DATA_MASK			(PIF_RC_PWM_DATA_SIZE - 1)


/**
 * @struct StPifRcPwmPulse
 * @brief
 */
typedef struct StPifRcPwmPulse
{
	uint32_t rising;
	uint32_t falling;
} PifRcPwmPulse;

/**
 * @struct StPifRcPwmData
 * @brief
 */
typedef struct StPifRcPwmData
{
	PifRcPwmPulse pulse[PIF_RC_PWM_DATA_SIZE];
	uint8_t ptr, last_ptr;
} PifRcPwmData;

/**
 * @struct StPifRcPwm
 * @brief Pulse를 관리하기 위한 구조체
 */
typedef struct StPifRcPwm
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Public Member Variable

	// Read-only Member Variable

	// Private Member Variable
	struct {
		BOOL check		: 1;
		uint16_t min	: 15;
		uint16_t max;
	} __valid_range;
	uint8_t __max_channel;
	uint8_t __process_step;
	PifRcPwmData* __p_data;
	uint16_t* __p_channel;
} PifRcPwm;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcPwm_Init
 * @brief RC PWM을 초기화한다.
 * @param p_owner
 * @param id
 * @param channel_count
 * @return 성공 여부를 반환한다.
 */
BOOL pifRcPwm_Init(PifRcPwm* p_owner, PifId id, uint8_t channel_count);

/**
 * @fn pifRcPwm_Clear
 * @brief PifRcPwm내에 할당 메모리를 반환한다.
 * @param p_owner RcPwm 자신
 */
void pifRcPwm_Clear(PifRcPwm* p_owner);

/**
 * @fn pifRcPwm_SetValidRange
 * @brief
 * @param p_owner
 * @param min
 * @param max
 * @return
 */
BOOL pifRcPwm_SetValidRange(PifRcPwm* p_owner, uint32_t min, uint32_t max);

/**
 * @fn pifRcPwm_ResetMeasureValue
 * @brief
 * @param p_owner
 */
void pifRcPwm_ResetMeasureValue(PifRcPwm* p_owner);

/**
 * @fn pifRcPwm_sigEdge
 * @param p_owner
 * @param state
 * @param time_us
 * @return
 */
uint16_t pifRcPwm_sigEdge(PifRcPwm* p_owner, uint8_t channel, PifPulseState state, uint32_t time_us);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_PWM_H
