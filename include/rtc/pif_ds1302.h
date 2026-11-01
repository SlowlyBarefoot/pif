// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ds1302.h
 * @brief DS1302 trickle-charge timekeeping chip on its 3-wire interface, as a PifRtc.
 *
 * The chip has three lines: CE, SCLK and a bidirectional I/O. They are driven from GPIO pins through act_write,
 * and I/O is read through act_read. Bits go least significant first, the chip latches I/O on the rising edge of
 * SCLK and drives it after the falling edge when it is read. act_io_mode turns the I/O pin of the MCU to input
 * before the chip drives it. It may be NULL when the pin is open-drain with a pull-up, since act_write then sets
 * it high to release it.
 *
 * The date and time are read and written in one clock burst, so they never mix the seconds before and after a
 * tick. Writes keep the write protect bit set between them, and set the clock running. The chip keeps hours in
 * either 12 or 24 hour mode; both are read, and they are always written in 24 hour mode.
 */
#ifndef PIF_DS1302_H
#define PIF_DS1302_H


#include "rtc/pif_rtc.h"


#define DS1302_RAM_SIZE			31		///< Bytes of the battery-backed RAM

#define DS1302_DEFAULT_DELAY	1		///< Default wait between the edges of SCLK, in microseconds
#define DS1302_CE_DELAY			4		///< Wait after CE goes high and after it goes low, in microseconds

// Values of the trickle charger register for pifDs1302_SetTrickleCharger()
#define DS1302_TC_DISABLE		0x00	///< Trickle charger off, as any TCS bits other than DS1302_TC_ENABLE leave it
#define DS1302_TC_ENABLE		0xA0	///< TCS bits that turn the trickle charger on, ORed with a diode and a resistor
#define DS1302_TC_DIODE_1		0x04	///< One diode
#define DS1302_TC_DIODE_2		0x08	///< Two diodes
#define DS1302_TC_RES_2K		0x01	///< 2 kohm
#define DS1302_TC_RES_4K		0x02	///< 4 kohm
#define DS1302_TC_RES_8K		0x03	///< 8 kohm


/**
 * @brief Lines of the 3-wire interface.
 */
typedef enum EnPifDs1302Pin
{
	DS1302_PIN_CE		= 0,	///< Chip enable, high during a transfer
	DS1302_PIN_SCLK		= 1,	///< Serial clock
	DS1302_PIN_IO		= 2		///< Data, driven by the MCU only while it is an output
} PifDs1302Pin;

/**
 * @brief Drives a line of the chip.
 */
typedef void (*PifActDs1302Write)(PifDs1302Pin pin, SWITCH state);
/**
 * @brief Reads the I/O line of the chip.
 */
typedef SWITCH (*PifActDs1302Read)();
/**
 * @brief Turns the I/O pin of the MCU to input or back to output.
 */
typedef void (*PifActDs1302IoMode)(BOOL input);


struct StPifDs1302;
typedef struct StPifDs1302 PifDs1302;

/**
 * @class StPifDs1302
 * @brief DS1302 timekeeping chip.
 */
struct StPifDs1302
{
	/// The parent variable must be at the beginning of this structure.
	PifRtc parent;

	// Private Member Variable
	uint8_t __delay1us;

	// Private Action Function
	PifActDs1302Write __act_write;
	PifActDs1302Read __act_read;
	PifActDs1302IoMode __act_io_mode;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the chip driver and sets CE and SCLK low. The chip is not accessed.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param act_write Drives CE, SCLK and I/O.
 * @param act_read Reads I/O.
 * @param act_io_mode Turns the I/O pin to input or output, or NULL for an open-drain pin.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when act_write or act_read is NULL.
 */
BOOL pifDs1302_Init(PifDs1302 *p_owner, PifId id, PifActDs1302Write act_write, PifActDs1302Read act_read,
	PifActDs1302IoMode act_io_mode);

/**
 * @brief Removes the sync task of the common layer.
 * @param p_owner Pointer to the owner instance.
 */
void pifDs1302_Clear(PifDs1302 *p_owner);

/**
 * @brief Changes the wait between the edges of SCLK. The chip takes 2 MHz at 5 V but only 500 kHz at 2 V, so a
 *        low supply needs 1 or more. 0 does not wait, for a port whose GPIO calls are slow enough by themselves.
 * @param p_owner Pointer to the owner instance.
 * @param delay1us Wait in microseconds. DS1302_DEFAULT_DELAY at the start.
 */
void pifDs1302_SetDelay(PifDs1302 *p_owner, uint8_t delay1us);

/**
 * @brief Checks whether the oscillator runs. It stops at the first power-on until the time is written.
 * @param p_owner Pointer to the owner instance.
 * @param p_running Receives TRUE when the clock runs.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when a parameter is NULL.
 */
BOOL pifDs1302_IsRunning(PifDs1302 *p_owner, BOOL *p_running);

/**
 * @brief Sets the trickle charger that charges a supercapacitor or a rechargeable battery on VCC1.
 * @param p_owner Pointer to the owner instance.
 * @param value DS1302_TC_DISABLE, or DS1302_TC_ENABLE ORed with one DS1302_TC_DIODE_ and one DS1302_TC_RES_.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when p_owner is NULL.
 */
BOOL pifDs1302_SetTrickleCharger(PifDs1302 *p_owner, uint8_t value);

/**
 * @brief Reads the battery-backed RAM.
 * @param p_owner Pointer to the owner instance.
 * @param addr First byte, 0 to DS1302_RAM_SIZE - 1.
 * @param p_data Receives the bytes.
 * @param size Number of bytes. addr + size must not exceed DS1302_RAM_SIZE.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when a parameter is out of range.
 */
BOOL pifDs1302_ReadRam(PifDs1302 *p_owner, uint8_t addr, uint8_t *p_data, uint8_t size);

/**
 * @brief Writes the battery-backed RAM.
 * @param p_owner Pointer to the owner instance.
 * @param addr First byte, 0 to DS1302_RAM_SIZE - 1.
 * @param p_data Bytes to write.
 * @param size Number of bytes. addr + size must not exceed DS1302_RAM_SIZE.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when a parameter is out of range.
 */
BOOL pifDs1302_WriteRam(PifDs1302 *p_owner, uint8_t addr, const uint8_t *p_data, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DS1302_H
