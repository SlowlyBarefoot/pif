#ifndef PIF_RC_PPM_H
#define PIF_RC_PPM_H


#include "rc/pif_rc.h"


#define PIF_RC_PPM_DATA_SIZE			4
#define PIF_RC_PPM_DATA_MASK			(PIF_RC_PPM_DATA_SIZE - 1)


/**
 * @struct StPifRcPpmPulse
 * @brief One captured PPM pulse edge pair.
 */
typedef struct StPifRcPpmPulse
{
	uint32_t rising;
	uint32_t falling;
} PifRcPpmPulse;

/**
 * @struct StPifRcPpm
 * @brief Runtime state for PPM decoding.
 */
typedef struct StPifRcPpm
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

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
 * @brief Initializes a PPM receiver instance.
 * @param p_owner Pointer to the PPM receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @param channel_count Number of channels to decode.
 * @param threshold_1us Frame separation threshold in microseconds.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcPpm_Init(PifRcPpm* p_owner, PifId id, uint8_t channel_count, uint16_t threshold_1us);

/**
 * @fn pifRcPpm_Clear
 * @brief Releases resources allocated by the PPM receiver instance.
 * @param p_owner Pointer to the PPM receiver object.
 */
void pifRcPpm_Clear(PifRcPpm* p_owner);

/**
 * @fn pifRcPpm_SetValidRange
 * @brief Sets the valid pulse width range for decoded channel values.
 * @param p_owner Pointer to the PPM receiver object.
 * @param min Minimum accepted pulse width in microseconds.
 * @param max Maximum accepted pulse width in microseconds.
 * @return TRUE after the range is applied.
 */
BOOL pifRcPpm_SetValidRange(PifRcPpm* p_owner, uint32_t min, uint32_t max);

/**
 * @fn pifRcPpm_ResetMeasureValue
 * @brief Resets buffered pulse timing measurements used by the decoder.
 * @param p_owner Pointer to the PPM receiver object.
 */
void pifRcPpm_ResetMeasureValue(PifRcPpm* p_owner);

/**
 * @fn pifRcPpm_sigTick
 * @brief Processes one captured pulse timestamp.
 * @param p_owner Pointer to the PPM receiver object.
 * @param time_us Timestamp in microseconds.
 * @return Decoded pulse width value, or 0 when no complete channel value is available.
 */
uint16_t pifRcPpm_sigTick(PifRcPpm* p_owner, uint32_t time_us);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_PPM_H
