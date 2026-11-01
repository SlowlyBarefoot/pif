// SPDX-License-Identifier: BSD-3-Clause
#include "rtc/pif_ds1302.h"


// Command bytes of the write; the read is one more.
#define DS1302_REG_SECONDS		0x80
#define DS1302_REG_CONTROL		0x8E
#define DS1302_REG_TRICKLE		0x90
#define DS1302_REG_CLOCK_BURST	0xBE
#define DS1302_REG_RAM			0xC0

#define DS1302_READ				0x01

#define DS1302_SEC_CH			0x80	///< Clock halt
#define DS1302_HOUR_12			0x80	///< 12 hour mode
#define DS1302_HOUR_PM			0x20	///< PM in 12 hour mode
#define DS1302_CONTROL_WP		0x80	///< Write protect

#define DS1302_CLOCK_SIZE		8		///< Registers of a clock burst, from the seconds to the control


/**
 * @fn _delay
 * @brief Waits between the edges of SCLK.
 * @param p_owner Pointer to the owner instance.
 */
static void _delay(PifDs1302 *p_owner)
{
	if (p_owner->__delay1us) pif_Delay1us(p_owner->__delay1us);
}

/**
 * @fn _start
 * @brief Begins a transfer. SCLK must be low when CE goes high.
 * @param p_owner Pointer to the owner instance.
 */
static void _start(PifDs1302 *p_owner)
{
	(*p_owner->__act_write)(DS1302_PIN_SCLK, OFF);
	(*p_owner->__act_write)(DS1302_PIN_CE, ON);
	pif_Delay1us(DS1302_CE_DELAY);
}

/**
 * @fn _stop
 * @brief Ends a transfer, which also releases I/O on the chip, and gives I/O back to the MCU.
 * @param p_owner Pointer to the owner instance.
 */
static void _stop(PifDs1302 *p_owner)
{
	(*p_owner->__act_write)(DS1302_PIN_CE, OFF);
	(*p_owner->__act_write)(DS1302_PIN_SCLK, OFF);
	if (p_owner->__act_io_mode) (*p_owner->__act_io_mode)(FALSE);
	pif_Delay1us(DS1302_CE_DELAY);
}

/**
 * @fn _writeByte
 * @brief Sends a byte, least significant bit first. The chip latches each bit on the rising edge of SCLK.
 * @param p_owner Pointer to the owner instance.
 * @param value Byte to send.
 * @param read_after TRUE for a read command: SCLK stays high after the last bit and I/O turns to input, as the
 *                   chip drives the first bit at the next falling edge.
 */
static void _writeByte(PifDs1302 *p_owner, uint8_t value, BOOL read_after)
{
	int i;

	for (i = 0; i < 8; i++) {
		(*p_owner->__act_write)(DS1302_PIN_IO, (value >> i) & 1);
		_delay(p_owner);
		(*p_owner->__act_write)(DS1302_PIN_SCLK, ON);
		_delay(p_owner);
		if (read_after && i == 7) {
			if (p_owner->__act_io_mode) {
				(*p_owner->__act_io_mode)(TRUE);
			}
			else {
				(*p_owner->__act_write)(DS1302_PIN_IO, ON);
			}
		}
		else {
			(*p_owner->__act_write)(DS1302_PIN_SCLK, OFF);
		}
	}
}

/**
 * @fn _readByte
 * @brief Receives a byte, least significant bit first. The chip drives each bit after a falling edge of SCLK.
 * @param p_owner Pointer to the owner instance.
 * @return The byte.
 */
static uint8_t _readByte(PifDs1302 *p_owner)
{
	uint8_t value = 0;
	int i;

	for (i = 0; i < 8; i++) {
		(*p_owner->__act_write)(DS1302_PIN_SCLK, OFF);
		_delay(p_owner);
		if ((*p_owner->__act_read)()) value |= 1 << i;
		(*p_owner->__act_write)(DS1302_PIN_SCLK, ON);
		_delay(p_owner);
	}
	return value;
}

/**
 * @fn _read
 * @brief Reads one register or a burst.
 * @param p_owner Pointer to the owner instance.
 * @param command Write command byte of the register or the burst.
 * @param p_data Receives the bytes.
 * @param size Number of bytes.
 */
static void _read(PifDs1302 *p_owner, uint8_t command, uint8_t *p_data, uint8_t size)
{
	uint8_t i;

	_start(p_owner);
	_writeByte(p_owner, command | DS1302_READ, TRUE);
	for (i = 0; i < size; i++) p_data[i] = _readByte(p_owner);
	_stop(p_owner);
}

/**
 * @fn _write
 * @brief Writes one register or a burst.
 * @param p_owner Pointer to the owner instance.
 * @param command Write command byte of the register or the burst.
 * @param p_data Bytes to write.
 * @param size Number of bytes.
 */
static void _write(PifDs1302 *p_owner, uint8_t command, const uint8_t *p_data, uint8_t size)
{
	uint8_t i;

	_start(p_owner);
	_writeByte(p_owner, command, FALSE);
	for (i = 0; i < size; i++) _writeByte(p_owner, p_data[i], FALSE);
	_stop(p_owner);
}

/**
 * @fn _writeProtect
 * @brief Sets or clears the write protect bit, which blocks every write but the one to the control register.
 * @param p_owner Pointer to the owner instance.
 * @param protect TRUE to set it.
 */
static void _writeProtect(PifDs1302 *p_owner, BOOL protect)
{
	uint8_t value = protect ? DS1302_CONTROL_WP : 0;

	_write(p_owner, DS1302_REG_CONTROL, &value, 1);
}

/**
 * @fn _isBcd
 * @brief Checks that both digits of a BCD byte are decimal.
 * @param value Byte to check.
 * @return TRUE when they are.
 */
static BOOL _isBcd(uint8_t value)
{
	return (value & 0x0F) <= 9 && (value >> 4) <= 9;
}

/**
 * @fn _actRead
 * @brief Reads the date and time in one clock burst.
 * @param p_rtc Pointer to the common layer of the instance.
 * @param p_datetime Receives the date and time.
 * @return TRUE on success. FALSE with E_INVALID_STATE when the clock is halted, or with E_WRONG_DATA when a
 *         register is out of range, as when the chip does not answer.
 */
static BOOL _actRead(PifRtc *p_rtc, PifDateTime *p_datetime)
{
	PifDs1302 *p_owner = (PifDs1302 *)p_rtc;
	uint8_t reg[7], hour;
	PifDateTime datetime;
	int i;

	_read(p_owner, DS1302_REG_CLOCK_BURST, reg, 7);

	if (reg[0] & DS1302_SEC_CH) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	hour = reg[2];
	reg[2] &= (hour & DS1302_HOUR_12) ? 0x1F : 0x3F;
	for (i = 0; i < 7; i++) {
		if (!_isBcd(reg[i])) {
			pif_error = E_WRONG_DATA;
			return FALSE;
		}
	}

	datetime.second = pifRtc_BcdToBin(reg[0]);
	datetime.minute = pifRtc_BcdToBin(reg[1]);
	datetime.hour = pifRtc_BcdToBin(reg[2]);
	if (hour & DS1302_HOUR_12) {
		if (datetime.hour < 1 || datetime.hour > 12) {
			pif_error = E_WRONG_DATA;
			return FALSE;
		}
		if (datetime.hour == 12) datetime.hour = 0;
		if (hour & DS1302_HOUR_PM) datetime.hour += 12;
	}
	datetime.day = pifRtc_BcdToBin(reg[3]);
	datetime.month = pifRtc_BcdToBin(reg[4]);
	datetime.year = pifRtc_BcdToBin(reg[6]);
	datetime.millisecond = 0;
	if (!pifRtc_CheckDateTime(&datetime)) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	*p_datetime = datetime;
	return TRUE;
}

/**
 * @fn _actWrite
 * @brief Writes the date and time in one clock burst, in 24 hour mode with the clock running. The day register
 *        gets 1 for Sunday to 7 for Saturday.
 * @param p_rtc Pointer to the common layer of the instance.
 * @param p_datetime Date and time to write.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when the year is beyond 2099.
 */
static BOOL _actWrite(PifRtc *p_rtc, const PifDateTime *p_datetime)
{
	PifDs1302 *p_owner = (PifDs1302 *)p_rtc;
	uint8_t reg[DS1302_CLOCK_SIZE];

	if (p_datetime->year > 99) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	reg[0] = pifRtc_BinToBcd(p_datetime->second);
	reg[1] = pifRtc_BinToBcd(p_datetime->minute);
	reg[2] = pifRtc_BinToBcd(p_datetime->hour);
	reg[3] = pifRtc_BinToBcd(p_datetime->day);
	reg[4] = pifRtc_BinToBcd(p_datetime->month);
	reg[5] = pifRtc_GetWeekday(p_datetime) + 1;
	reg[6] = pifRtc_BinToBcd(p_datetime->year);
	reg[7] = DS1302_CONTROL_WP;		// The burst has to write all eight, and sets the protection back last.

	_writeProtect(p_owner, FALSE);
	_write(p_owner, DS1302_REG_CLOCK_BURST, reg, DS1302_CLOCK_SIZE);
	return TRUE;
}

BOOL pifDs1302_Init(PifDs1302 *p_owner, PifId id, PifActDs1302Write act_write, PifActDs1302Read act_read,
	PifActDs1302IoMode act_io_mode)
{
	if (!p_owner || !act_write || !act_read) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifDs1302));

	if (!pifRtc_Init(&p_owner->parent, id, _actRead, _actWrite)) return FALSE;
	p_owner->__delay1us = DS1302_DEFAULT_DELAY;
	p_owner->__act_write = act_write;
	p_owner->__act_read = act_read;
	p_owner->__act_io_mode = act_io_mode;

	(*act_write)(DS1302_PIN_CE, OFF);
	(*act_write)(DS1302_PIN_SCLK, OFF);
	return TRUE;
}

void pifDs1302_Clear(PifDs1302 *p_owner)
{
	pifRtc_Clear(&p_owner->parent);
}

void pifDs1302_SetDelay(PifDs1302 *p_owner, uint8_t delay1us)
{
	p_owner->__delay1us = delay1us;
}

BOOL pifDs1302_IsRunning(PifDs1302 *p_owner, BOOL *p_running)
{
	uint8_t second;

	if (!p_owner || !p_running) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_read(p_owner, DS1302_REG_SECONDS, &second, 1);
	*p_running = !(second & DS1302_SEC_CH);
	return TRUE;
}

BOOL pifDs1302_SetTrickleCharger(PifDs1302 *p_owner, uint8_t value)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_writeProtect(p_owner, FALSE);
	_write(p_owner, DS1302_REG_TRICKLE, &value, 1);
	_writeProtect(p_owner, TRUE);
	return TRUE;
}

BOOL pifDs1302_ReadRam(PifDs1302 *p_owner, uint8_t addr, uint8_t *p_data, uint8_t size)
{
	uint8_t i;

	if (!p_owner || !p_data || addr >= DS1302_RAM_SIZE || size > DS1302_RAM_SIZE - addr) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	for (i = 0; i < size; i++) _read(p_owner, DS1302_REG_RAM + ((addr + i) << 1), &p_data[i], 1);
	return TRUE;
}

BOOL pifDs1302_WriteRam(PifDs1302 *p_owner, uint8_t addr, const uint8_t *p_data, uint8_t size)
{
	uint8_t i;

	if (!p_owner || !p_data || addr >= DS1302_RAM_SIZE || size > DS1302_RAM_SIZE - addr) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_writeProtect(p_owner, FALSE);
	for (i = 0; i < size; i++) _write(p_owner, DS1302_REG_RAM + ((addr + i) << 1), &p_data[i], 1);
	_writeProtect(p_owner, TRUE);
	return TRUE;
}
