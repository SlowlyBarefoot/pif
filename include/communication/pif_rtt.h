// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rtt.h
 * @brief A PifUart on a SEGGER RTT channel: the log, a terminal or a capture over the debug probe.
 *
 * SEGGER RTT (Real Time Transfer) moves bytes through ring buffers in the RAM of the target, which the debug
 * probe reads and writes in the background over SWD. Writing is a memcpy, so it costs the CPU little and is
 * many times faster than a UART, but it needs a probe attached (J-Link, or OpenOCD and pyOCD with others).
 *
 * The RTT code is not part of pif. Add SEGGER_RTT.c of it to the build, put SEGGER_RTT.h and SEGGER_RTT_Conf.h
 * on the include path and define PIF_USE_SEGGER_RTT in pif_conf.h. Without PIF_USE_SEGGER_RTT this compiles
 * to nothing, so a build that takes every source of pif does not need RTT.
 *
 *     PifRtt rtt;
 *
 *     pifRtt_Init(&rtt, PIF_ID_AUTO, 0, 256);              // channel 0, 256 bytes of TX buffer
 *     pifUart_AttachTxTask(&rtt.parent, PIF_ID_AUTO, TM_PERIOD, 1000, "RttTx");
 *     pifUart_AttachRxTask(&rtt.parent, PIF_ID_AUTO, TM_PERIOD, 10000, "RttRx");
 *     pifLog_AttachUart(&rtt.parent, 256);                 // or a capture, a trace, a protocol
 *
 * The UART sends from its TX buffer in its TX task: each run copies into the up buffer of the channel as much
 * as it has room for, and the rest waits for the next run. Nothing is ever lost behind the TX buffer, and a
 * write never blocks, whatever mode the up buffer is in. Received bytes are read from the down buffer by
 * the RX task through act_receive_data.
 *
 * Channel 0 is set up by RTT itself. Another channel needs its buffers: pifRtt_ConfigBuffers().
 */
#ifndef PIF_RTT_H
#define PIF_RTT_H


#include "core/pif.h"

#ifdef PIF_USE_SEGGER_RTT

#include "communication/pif_uart.h"


/// Baud rate the UART is set up with. Only the periods of its tasks follow it: 10 us a byte.
#ifndef PIF_RTT_BAUDRATE
#define PIF_RTT_BAUDRATE		1000000UL
#endif


/**
 * @class StPifRtt
 * @brief A UART on one RTT channel. parent is the PifUart to hand to the log or a protocol.
 */
typedef struct StPifRtt
{
	PifUart parent;			///< Must stay the first member.

	// Read-only Member Variable
	uint8_t _index;			///< RTT channel (buffer index)
} PifRtt;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a UART on an RTT channel, with a TX buffer. Attach its tasks afterwards.
 * @param id Logical ID, or PIF_ID_AUTO.
 * @param index RTT channel, below SEGGER_RTT_MAX_NUM_UP_BUFFERS.
 * @param tx_size Size of the TX buffer.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRtt_Init(PifRtt *p_owner, PifId id, uint8_t index, uint16_t tx_size);

/**
 * @brief Gives the up and down buffers of a channel other than 0 and sets the up buffer to skip a write that
 *        does not fit, which pifRtt never makes. A NULL buffer leaves that direction as it is.
 * @param p_name Name the probe shows for the channel. It must stay valid.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRtt_ConfigBuffers(PifRtt *p_owner, const char *p_name, uint8_t *p_up, uint16_t up_size, uint8_t *p_down,
		uint16_t down_size);

#ifdef __cplusplus
}
#endif

#endif	// PIF_USE_SEGGER_RTT

#endif	// PIF_RTT_H
