// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_usb_cdc.h
 * @brief A PifUart on a USB CDC ACM port (virtual COM port) of any USB device stack.
 *
 * The USB stack is not part of pif. The platform gives the few calls of its stack as act functions, and
 * pifUsbCdc does the rest the same way for every stack: it sends from the TX buffer of the UART in packets
 * the stack has room for, drops or holds what is sent while no terminal has the port open, and takes the
 * received bytes either by polling (act_read) or from the receive callback of the stack (pifUsbCdc_SigReceive).
 *
 *     PifUsbCdc cdc;
 *
 *     pifUsbCdc_Init(&cdc, PIF_ID_AUTO, 512, 0);           // 512 bytes of TX buffer, received bytes polled
 *     cdc.act_get_room = _cdcRoom;                         // what the stack takes now
 *     cdc.act_write = _cdcWrite;
 *     cdc.act_read = _cdcRead;
 *     cdc.act_is_open = _cdcIsOpen;                        // DTR
 *     pifUart_AttachTxTask(&cdc.parent, PIF_ID_AUTO, TM_PERIOD, 1000, "CdcTx");
 *     pifUart_AttachRxTask(&cdc.parent, PIF_ID_AUTO, TM_PERIOD, 1000, "CdcRx");
 *     pifLog_AttachUart(&cdc.parent, 256);                 // or a capture, a trace, a protocol
 *
 * With TinyUSB, define PIF_USE_TINYUSB in pif_conf.h and call pifUsbCdc_InitTinyUsb() instead: it sets the act
 * functions for an interface of TinyUSB. The application still calls tud_task() in its loop.
 *
 * Stacks whose transmit takes a pointer and sends from it later, as the CDC class of the STM32 USB device
 * library does with CDC_Transmit_FS(), are handed a copy in packet, which act_get_room must keep from being
 * written while the stack sends it: 0 while the stack is busy, PIF_USB_CDC_PACKET_SIZE when it is free.
 * pifUsbCdc_SigTxComplete() from the transmit complete callback then sends the next packet at once.
 *
 * The baud rate and line settings the terminal sets mean nothing on USB and are left to the stack. The UART
 * is set up with PIF_USB_CDC_BAUDRATE, which only the periods of its tasks follow.
 */
#ifndef PIF_USB_CDC_H
#define PIF_USB_CDC_H


#include "communication/pif_uart.h"


/// Most bytes handed to act_write at once, the bulk packet of a full speed device.
#ifndef PIF_USB_CDC_PACKET_SIZE
#define PIF_USB_CDC_PACKET_SIZE		64
#endif

/// Baud rate the UART is set up with: 1 us a byte, near a full speed bulk endpoint.
#ifndef PIF_USB_CDC_BAUDRATE
#define PIF_USB_CDC_BAUDRATE		10000000UL
#endif


struct StPifUsbCdc;
typedef struct StPifUsbCdc PifUsbCdc;

/** @brief Bytes the stack takes now, 0 when it takes none. */
typedef uint16_t (*PifActUsbCdcGetRoom)(PifUsbCdc *p_cdc);
/** @brief Hands bytes to the stack. Returns the bytes it took. */
typedef uint16_t (*PifActUsbCdcWrite)(PifUsbCdc *p_cdc, uint8_t *p_data, uint16_t size);
/** @brief Sends what the stack holds now, for stacks that wait for a full packet. */
typedef void (*PifActUsbCdcFlush)(PifUsbCdc *p_cdc);
/** @brief Reads received bytes. Returns the bytes read. */
typedef uint16_t (*PifActUsbCdcRead)(PifUsbCdc *p_cdc, uint8_t *p_data, uint16_t size);
/** @brief TRUE while a terminal has the port open: the host set DTR and the device is configured. */
typedef BOOL (*PifActUsbCdcIsOpen)(PifUsbCdc *p_cdc);

/**
 * @class StPifUsbCdc
 * @brief A UART on a USB CDC port. parent is the PifUart to hand to the log or a protocol.
 */
struct StPifUsbCdc
{
	PifUart parent;			///< Must stay the first member.

	// Public Member Variable
	BOOL hold_while_closed;	///< FALSE (default): what is sent while the port is closed is dropped, as a UART
							///< with no one on the line loses it. TRUE: it waits in the TX buffer.
	uint8_t itf;			///< Interface number of the stack, for the act functions

	// Public Action Function
	PifActUsbCdcGetRoom act_get_room;	///< NULL: act_write takes what it can.
	PifActUsbCdcWrite act_write;		///< Needed.
	PifActUsbCdcFlush act_flush;		///< May be NULL.
	PifActUsbCdcRead act_read;			///< Polled by the RX task when pifUsbCdc_Init() got no RX buffer.
	PifActUsbCdcIsOpen act_is_open;		///< NULL: always open.

	// Read-only Member Variable
	uint32_t _tx_dropped;	///< Bytes dropped while the port was closed
	uint8_t _packet[PIF_USB_CDC_PACKET_SIZE];	///< Copy handed to act_write
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a UART on a USB CDC port, with a TX buffer and, when rx_size is not 0, an RX buffer for
 *        pifUsbCdc_SigReceive(). Set the act functions and attach the tasks of the UART afterwards.
 * @param id Logical ID, or PIF_ID_AUTO.
 * @param tx_size Size of the TX buffer.
 * @param rx_size Size of the RX buffer, or 0 when the received bytes are polled with act_read.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifUsbCdc_Init(PifUsbCdc *p_owner, PifId id, uint16_t tx_size, uint16_t rx_size);

/**
 * @brief Received bytes from the receive callback of the stack, an interrupt included. They go in all or none.
 * @return FALSE when the RX buffer has no room for them. A stack that can hold the host back, as the STM32
 *         library does until USBD_CDC_ReceivePacket(), should arm the next packet only once
 *         pifUsbCdc_GetRxRoom() is a packet or more.
 */
BOOL pifUsbCdc_SigReceive(PifUsbCdc *p_owner, uint8_t *p_data, uint16_t length);

/** @brief Room of the RX buffer. */
uint16_t pifUsbCdc_GetRxRoom(PifUsbCdc *p_owner);

/** @brief From the transmit complete callback of the stack: the next packet goes at once. */
void pifUsbCdc_SigTxComplete(PifUsbCdc *p_owner);

/** @brief TRUE while a terminal has the port open. */
BOOL pifUsbCdc_IsOpen(PifUsbCdc *p_owner);

#ifdef PIF_USE_TINYUSB

/**
 * @brief pifUsbCdc_Init() with the act functions of an interface of TinyUSB (CFG_TUD_CDC of tusb_config.h).
 * @param itf CDC interface of TinyUSB, 0 for the first.
 */
BOOL pifUsbCdc_InitTinyUsb(PifUsbCdc *p_owner, PifId id, uint8_t itf, uint16_t tx_size);

#endif

#ifdef __cplusplus
}
#endif


#endif	// PIF_USB_CDC_H
