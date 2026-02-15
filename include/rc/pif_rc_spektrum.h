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
 * @brief Runtime state for Spektrum frame decoding.
 */
typedef struct StPifRcSpektrum
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Read-only Member Variable
	uint8_t _protocol_id;
	double _pos_factor;

	// Private Member Variable
	PifUart* __p_uart;
    uint8_t __p_buffer[SPEKTRUM_FRAME_SIZE];
    uint8_t __index;    // Current message length.
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
 * @brief Initializes a Spektrum receiver instance.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @param protocol_id Spektrum protocol identifier.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcSpektrum_Init(PifRcSpektrum* p_owner, PifId id, uint8_t protocol_id);

/**
 * @fn pifRcSpektrum_AttachUart
 * @brief Attaches a UART interface to the Spektrum parser.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcSpektrum_AttachUart(PifRcSpektrum* p_owner, PifUart* p_uart);

/**
 * @fn pifRcSpektrum_DetachUart
 * @brief Detaches the UART interface from the Spektrum parser.
 * @param p_owner Pointer to the Spektrum receiver object.
 */
void pifRcSpektrum_DetachUart(PifRcSpektrum* p_owner);

/**
 * @fn pifRcSpektrum_CheckFailSafe
 * @brief Checks whether Spektrum failsafe is active.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @return TRUE if failsafe is active, otherwise FALSE.
 */
BOOL pifRcSpektrum_CheckFailSafe(PifRcSpektrum* p_owner);

/**
 * @fn pifRcSpektrum_SendFrame
 * @brief Encodes and transmits one Spektrum frame.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @param p_channel Pointer to channel values.
 * @param count Number of channel values available in p_channel.
 * @return TRUE if the frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcSpektrum_SendFrame(PifRcSpektrum* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SPEKTRUM_H
