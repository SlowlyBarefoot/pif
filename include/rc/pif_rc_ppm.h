#ifndef PIF_RC_PPM_H
#define PIF_RC_PPM_H


#include "rc/pif_rc.h"


#define PIF_RC_PPM_DATA_SIZE			4
#define PIF_RC_PPM_DATA_MASK			(PIF_RC_PPM_DATA_SIZE - 1)


/**
 * @struct StPifRcPpmPulse
 * @brief
 */
typedef struct StPifRcPpmPulse
{
	uint32_t rising;
	uint32_t falling;
} PifRcPpmPulse;

/**
 * @struct StPifRcPpm
 * @brief RC PPM를 관리하기 위한 구조체
 */
typedef struct StPifRcPpm
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Public Member Variable

	// Read-only Member Variable
	int8_t _channel;

	// Private Member Variable
	struct {
		BOOL check		: 1;
		uint16_t min	: 15;
		uint16_t max;
	} __valid_range;
	PifRcPpmPulse __pulse[PIF_RC_PPM_DATA_SIZE];
	uint8_t __ptr, __last_ptr, __count;
	uint8_t __max_channel;
	uint8_t __process_step;
	uint16_t __threshold_1us;
	uint16_t* __p_channel;
} PifRcPpm;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcPpm_Init
 * @brief RC PPM를 초기화한다.
 * @param p_owner
 * @param id
 * @param channel_count
 * @param threshold_1us
 * @return 성공 여부를 반환한다.
 */
BOOL pifRcPpm_Init(PifRcPpm* p_owner, PifId id, uint8_t channel_count, uint16_t threshold_1us);

/**
 * @fn pifRcPpm_Clear
 * @brief PifRcPpm내에 할당 메모리를 반환한다.
 * @param p_owner RcPpm 자신
 */
void pifRcPpm_Clear(PifRcPpm* p_owner);

/**
 * @fn pifRcPpm_SetValidRange
 * @brief
 * @param p_owner
 * @param min
 * @param max
 * @return
 */
BOOL pifRcPpm_SetValidRange(PifRcPpm* p_owner, uint32_t min, uint32_t max);

/**
 * @fn pifRcPpm_ResetMeasureValue
 * @brief
 * @param p_owner
 */
void pifRcPpm_ResetMeasureValue(PifRcPpm* p_owner);

/**
 * @fn pifRcPpm_sigTick
 * @brief
 * @param p_owner
 * @param time_us
 * @return
 */
uint16_t pifRcPpm_sigTick(PifRcPpm* p_owner, uint32_t time_us);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_PPM_H
