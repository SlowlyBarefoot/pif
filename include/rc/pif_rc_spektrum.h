#ifndef PIF_RC_SPEKTRUM_H
#define PIF_RC_SPEKTRUM_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SPEKTRUM_CHANNEL_COUNT  				8

#define PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2		0x01
#define PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSM2		0x12
#define PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS		0xA2
#define PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX		0xB2

#define SPEKTRUM_FRAME_SIZE   						16


/**
 * @class StPifRcSpektrum
 * @brief
 */
typedef struct StPifRcSpektrum
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Public Member Variable

	// Read-only Member Variable
	uint8_t _protocol_id;
	double _pos_factor;

	// Private Member Variable
	PifUart* __p_uart;
    uint8_t __p_buffer[SPEKTRUM_FRAME_SIZE];
	uint8_t __index;                            // message length
	uint8_t __id_mask;
	uint8_t __id_shift;
	uint8_t __pos_mask;
    uint16_t __channel[PIF_SPEKTRUM_CHANNEL_COUNT];
	uint32_t __last_time;
} PifRcSpektrum;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcSpektrum_Init
 * @brief
 * @param p_owner
 * @param id
 * @param protocol_id
 * @return
 */
BOOL pifRcSpektrum_Init(PifRcSpektrum* p_owner, PifId id, uint8_t protocol_id);

/**
 * @fn pifRcSpektrum_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifRcSpektrum_AttachUart(PifRcSpektrum* p_owner, PifUart* p_uart);

/**
 * @fn pifRcSpektrum_DetachUart
 * @brief
 * @param p_owner
 */
void pifRcSpektrum_DetachUart(PifRcSpektrum* p_owner);

/**
 * @fn pifRcSpektrum_CheckFailSafe
 * @brief function used to check the failsafe status
 * @param p_owner
 * @return
 */
BOOL pifRcSpektrum_CheckFailSafe(PifRcSpektrum* p_owner);

/**
 * @fn pifRcSpektrum_SendFrame
 * @brief
 * @param p_owner
 * @param p_channel
 * @param count
 * @return
 */
BOOL pifRcSpektrum_SendFrame(PifRcSpektrum* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SPEKTRUM_H
