/**
 * @file pif_link_fragment.h
 * @brief Sends a request larger than one frame of a PifLink in fragments.
 *
 * Fragmentation is enabled per link with pifLink_EnableFragment(), so on a device with several links only the
 * links that need it pay for it. A link without it keeps a NULL pointer, and when no link enables it the code
 * in pif_link_fragment.c is not linked in.
 *
 * pifLink_MakeLargeRequest() sends data of any command in fragments of up to the size set with
 * pifLink_SetTxFragmentSize(). Each fragment is a request of its own with LINK_F_FRAGMENT and a new packet_id, so
 * retries and duplicate detection work per fragment. Its data starts with one fragment byte:
 * @code
 * fragment byte = (last ? 0x80 : 0) | seq      seq: 0 for the first fragment, then 1 to 127 and back to 1
 * @endcode
 * The next fragment is sent only after the previous one is answered, so all but the last fragment always
 * expect an answer. The receiver answers them with an empty answer by itself, and the answer to the last
 * fragment is the answer to the whole request. An LINK_F_ERROR answer to any fragment fails the whole request.
 *
 * The receiver hands the fragments to evt_answer as chosen by the mode given to pifLink_EnableFragment().
 * It takes one fragmented question at a time: a first fragment discards an unfinished one. A fragmented
 * question cannot be broadcast, and only questions are fragmented, never answers.
 */
#ifndef PIF_LINK_FRAGMENT_H
#define PIF_LINK_FRAGMENT_H


#include "protocol/pif_link.h"


// Default largest data size of one fragment sent by pifLink_MakeLargeRequest(), without its fragment byte.
// The fragment with its fragment byte has to fit in the receive packet of the other end.
// It can be changed later with pifLink_SetTxFragmentSize().
#ifndef PIF_LINK_TX_FRAGMENT_SIZE
#define PIF_LINK_TX_FRAGMENT_SIZE	(PIF_LINK_RX_PACKET_SIZE - 1)
#endif


/**
 * @brief How fragmented questions are handed to evt_answer.
 */
typedef enum EnPifLinkFragmentMode
{
	LINK_FM_NONE			= 0,	// Fragmented questions are answered with LINK_E_UNSUPPORTED. Large requests can be sent.
	LINK_FM_STREAM			= 1,	// evt_answer is called for every fragment with its offset and more.
	LINK_FM_REASSEMBLE		= 2		// evt_answer is called once with the whole data after the last fragment.
} PifLinkFragmentMode;

/**
 * @class StPifLinkFragment
 * @brief Fragment state of a link, allocated by pifLink_EnableFragment().
 */
typedef struct StPifLinkFragment
{
	// Sending
	PifRingBuffer tx_buffer;		// Frame of the fragment being sent. Allocated by the first large request.
	uint16_t tx_size;				// Largest data size of a fragment, without the fragment byte.
	uint8_t *p_tx_data;				// Data of the large request in progress.
	uint16_t tx_total;				// Data size of the large request in progress.
	uint16_t tx_offset;				// Data bytes sent in the fragments before the current one.
	uint16_t tx_chunk;				// Data bytes in the current fragment.
	uint8_t tx_seq;					// seq of the current fragment.

	// Receiving
	PifLinkFragmentMode rx_mode;
	BOOL rx_active;					// A fragmented question is being received.
	uint8_t rx_command;				// command of the fragmented question being received.
	uint8_t rx_src_id;				// src_id of the fragmented question being received.
	uint8_t rx_seq;					// seq expected in the next fragment.
	uint16_t rx_offset;				// Data bytes of the fragmented question received so far.
	uint8_t *p_message;				// LINK_FM_REASSEMBLE: whole data of the fragmented question.
	uint16_t message_size;			// Size of p_message.
	PifLinkPacket message;			// LINK_FM_REASSEMBLE: reassembled question passed to evt_answer.
} PifLinkFragment;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifLink_EnableFragment
 * @brief Enables fragmentation on a link, or changes its receive mode when it is already enabled. A fragmented
 *        question being received is dropped. The state is freed by pifLink_Clear().
 * @param p_owner Pointer to the link.
 * @param rx_mode How fragmented questions are handed to evt_answer.
 * @param message_size LINK_FM_REASSEMBLE: largest whole data size, 1 or above. A buffer of this size is allocated.
 *        Ignored for the other modes, which free the buffer.
 * @return TRUE on success, FALSE with pif_error set otherwise. The old mode is kept on failure.
 */
BOOL pifLink_EnableFragment(PifLink *p_owner, PifLinkFragmentMode rx_mode, uint16_t message_size);

/**
 * @fn pifLink_SetTxFragmentSize
 * @brief Changes the largest data size of a fragment sent by pifLink_MakeLargeRequest().
 *        The default is PIF_LINK_TX_FRAGMENT_SIZE.
 * @param p_owner Pointer to the link.
 * @param fragment_size Data bytes per fragment without the fragment byte, 1 to LINK_MAX_DATA_SIZE - 1.
 *        The fragment with its fragment byte has to fit in the receive packet of the other end.
 * @return TRUE on success, FALSE with pif_error set when fragmentation is not enabled, the size is invalid or
 *         a large request is in progress.
 */
BOOL pifLink_SetTxFragmentSize(PifLink *p_owner, uint16_t fragment_size);

/**
 * @fn pifLink_MakeLargeRequest
 * @brief Queues a request whose data is sent in fragments. Queued requests, large or not, are sent one at a
 *        time, each after the previous one is finished.
 * @param p_owner Pointer to the link.
 * @param dst_id LINK_T_MULTI: address of the receiver. LINK_BROADCAST is not allowed. Ignored for LINK_T_SINGLE.
 * @param p_request Request to send. LINK_F_RESPONSE_NO applies to the last fragment only. The link keeps this
 *        pointer until the request is finished.
 * @param p_data Data to send. It is not copied: it is read while each fragment is made, so it must stay
 *        unchanged until evt_response or evt_error is called, or until the last fragment is sent with
 *        LINK_F_RESPONSE_NO.
 * @param data_size Data size, 1 to 65535.
 * @return TRUE when queued, FALSE with pif_error set when fragmentation is not enabled, a parameter is invalid,
 *         no UART is attached or the request buffer is full.
 */
BOOL pifLink_MakeLargeRequest(PifLink *p_owner, uint8_t dst_id, const PifLinkRequest *p_request, uint8_t *p_data,
		uint16_t data_size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LINK_FRAGMENT_H
