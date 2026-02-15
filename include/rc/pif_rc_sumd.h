#ifndef PIF_RC_SUMD_H
#define PIF_RC_SUMD_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SUMD_CHANNEL_COUNT  32  // Must be less than or equal to 32.

// SUMD protocol constants.
#define SUMD_HEADER_SIZE        3
#define SUMD_CRC_SIZE           2

// SUMD decoding buffer size (+1 for ring buffer full/empty differentiation).
#define SUMD_FRAME_SIZE         (SUMD_HEADER_SIZE + PIF_SUMD_CHANNEL_COUNT * 2 + SUMD_CRC_SIZE + 1)


/**
 * @class StPifRcSumd
 * @brief Runtime state for SUMD frame decoding.
 */
typedef struct StPifRcSumd
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Private Member Variable
    PifUart* __p_uart;
    uint8_t __p_buffer[SUMD_FRAME_SIZE];
    uint8_t __index;    // Current message length.
    uint32_t __last_time;
} PifRcSumd;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcSumd_Init
 * @brief Initializes a SUMD receiver instance.
 * @param p_owner Pointer to the SUMD receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcSumd_Init(PifRcSumd* p_owner, PifId id);

/**
 * @fn pifRcSumd_AttachUart
 * @brief Attaches a UART interface to the SUMD parser.
 * @param p_owner Pointer to the SUMD receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcSumd_AttachUart(PifRcSumd* p_owner, PifUart* p_uart);

/**
 * @fn pifRcSumd_DetachUart
 * @brief Detaches the UART interface from the SUMD parser.
 * @param p_owner Pointer to the SUMD receiver object.
 */
void pifRcSumd_DetachUart(PifRcSumd* p_owner);

/**
 * @fn pifRcSumd_CheckFailSafe
 * @brief Checks whether SUMD failsafe is active.
 * @param p_owner Pointer to the SUMD receiver object.
 * @return TRUE if failsafe is active, otherwise FALSE.
 */
BOOL pifRcSumd_CheckFailSafe(PifRcSumd* p_owner);

/**
 * @fn pifRcSumd_SendFrame
 * @brief Encodes and transmits one SUMD frame.
 * @param p_owner Pointer to the SUMD receiver object.
 * @param p_channel Pointer to channel values.
 * @param count Number of channel values available in p_channel.
 * @return TRUE if the frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcSumd_SendFrame(PifRcSumd* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SUMD_H
