// SPDX-License-Identifier: BSD-3-Clause
#include "storage/pif_sd_spi.h"

#include <string.h>


#define R1_IDLE				0x01
#define R1_ILLEGAL			0x04
#define R1_NONE				0xFF

#define TOKEN_START			0xFE	///< Data token of CMD17, CMD18, CMD24 and of the CSD
#define TOKEN_MULTI_WRITE	0xFC	///< Data token of each sector of CMD25
#define TOKEN_STOP			0xFD	///< Ends CMD25
#define DATA_ACCEPTED		0x05	///< Data response "accepted", low 5 bits

#define ACMD(N)				(0x80 | (N))	///< Sent after CMD55


static uint8_t _xfer(PifSdSpi *p_owner, uint8_t out)
{
	uint8_t in;

	(*p_owner->__act_transfer)(p_owner, &out, &in, 1);
	return in;
}

static void _deselect(PifSdSpi *p_owner)
{
	(*p_owner->__act_select)(p_owner, FALSE);
	_xfer(p_owner, 0xFF);		// the card lets go of MISO only on a clock with the chip select high
}

/**
 * @fn _waitReady
 * @brief Waits while the card holds MISO low (busy).
 */
static BOOL _waitReady(PifSdSpi *p_owner, uint16_t timeout_ms)
{
	uint32_t start = pif_cumulative_timer1ms;

	do {
		if (_xfer(p_owner, 0xFF) == 0xFF) return TRUE;
	} while (!PIF_CHECK_ELAPSE_TIME_1MS(start, timeout_ms));
	return FALSE;
}

static BOOL _select(PifSdSpi *p_owner)
{
	(*p_owner->__act_select)(p_owner, TRUE);
	_xfer(p_owner, 0xFF);
	if (_waitReady(p_owner, p_owner->write_timeout)) return TRUE;
	_deselect(p_owner);
	return FALSE;
}

/**
 * @fn _cmd
 * @brief Sends a command and returns R1, R1_NONE when nothing answered. The card is left selected for what follows.
 */
static uint8_t _cmd(PifSdSpi *p_owner, uint8_t cmd, uint32_t arg)
{
	uint8_t frame[6], r1 = R1_NONE;
	int i;

	if (cmd & 0x80) {
		cmd &= 0x7F;
		r1 = _cmd(p_owner, 55, 0);
		if (r1 > R1_IDLE) return r1;
	}
	_deselect(p_owner);
	if (!_select(p_owner)) return R1_NONE;

	frame[0] = 0x40 | cmd;
	frame[1] = (uint8_t)(arg >> 24);
	frame[2] = (uint8_t)(arg >> 16);
	frame[3] = (uint8_t)(arg >> 8);
	frame[4] = (uint8_t)arg;
	frame[5] = cmd == 0 ? 0x95 : cmd == 8 ? 0x87 : 0x01;		// the only CRCs SPI mode checks
	(*p_owner->__act_transfer)(p_owner, frame, NULL, sizeof(frame));
	if (cmd == 12) _xfer(p_owner, 0xFF);		// stuff byte after the stop command

	// The answer comes within 8 bytes; bit 7 is clear in it.
	for (i = 0; i < 10; i++) {
		r1 = _xfer(p_owner, 0xFF);
		if (!(r1 & 0x80)) break;
	}
	return r1;
}

static BOOL _fail(PifSdSpi *p_owner, PifSdSpiStep step, uint8_t r1, PifError error)
{
	p_owner->_fail_step = step;
	p_owner->_fail_r1 = r1;
	pif_error = error;
	_deselect(p_owner);
	return FALSE;
}

/**
 * @fn _readData
 * @brief Waits for the start token and reads a data block and its CRC, which is not checked.
 */
static BOOL _readData(PifSdSpi *p_owner, uint8_t *p_data, size_t size)
{
	uint32_t start = pif_cumulative_timer1ms;
	uint8_t token;

	do {
		token = _xfer(p_owner, 0xFF);
		if (token != 0xFF) break;
	} while (!PIF_CHECK_ELAPSE_TIME_1MS(start, p_owner->read_timeout));
	if (token != TOKEN_START) return FALSE;
	(*p_owner->__act_transfer)(p_owner, NULL, p_data, size);
	(*p_owner->__act_transfer)(p_owner, NULL, NULL, 2);
	return TRUE;
}

/**
 * @fn _sectorsFromCsd
 * @brief Capacity from the CSD register, in sectors.
 */
static uint32_t _sectorsFromCsd(const uint8_t *p_csd)
{
	uint32_t c_size, mult, read_bl_len;

	if ((p_csd[0] >> 6) == 1) {
		// CSD version 2: (C_SIZE + 1) x 512KB
		c_size = ((uint32_t)(p_csd[7] & 0x3F) << 16) | ((uint32_t)p_csd[8] << 8) | p_csd[9];
		return (c_size + 1) << 10;
	}
	// CSD version 1: (C_SIZE + 1) x 2^(C_SIZE_MULT + 2) x 2^READ_BL_LEN bytes
	read_bl_len = p_csd[5] & 0x0F;
	c_size = ((uint32_t)(p_csd[6] & 0x03) << 10) | ((uint32_t)p_csd[7] << 2) | (p_csd[8] >> 6);
	mult = ((p_csd[9] & 0x03) << 1) | (p_csd[10] >> 7);
	return (c_size + 1) << (mult + 2 + read_bl_len - 9);
}

BOOL pifSdSpi_Init(PifSdSpi *p_owner, PifId id, PifActSdSpiSelect act_select, PifActSdSpiTransfer act_transfer,
		PifActSdSpiSetClock act_set_clock)
{
	if (!p_owner || !act_select || !act_transfer) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSdSpi));
	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->init_timeout = 1000;
	p_owner->read_timeout = 200;
	p_owner->write_timeout = 500;
	p_owner->_fail_r1 = R1_NONE;
	p_owner->__act_select = act_select;
	p_owner->__act_transfer = act_transfer;
	p_owner->__act_set_clock = act_set_clock;
	return TRUE;
}

BOOL pifSdSpi_Start(PifSdSpi *p_owner)
{
	uint8_t r1 = R1_NONE, buf[16];
	uint32_t start, arg;
	int i;

	p_owner->_type = SDST_NONE;
	p_owner->_sectors = 0;
	p_owner->_ocr = 0;
	p_owner->_fail_step = SDSS_NONE;
	p_owner->_fail_r1 = R1_NONE;
	if (p_owner->__act_set_clock) (*p_owner->__act_set_clock)(p_owner, FALSE);

	// 74 clocks or more with the chip select high wake the card.
	(*p_owner->__act_select)(p_owner, FALSE);
	(*p_owner->__act_transfer)(p_owner, NULL, NULL, 10);

	// CMD0 resets it into SPI mode. A card left in the middle of a transfer may need a few.
	for (i = 0; i < 5; i++) {
		r1 = _cmd(p_owner, 0, 0);
		if (r1 == R1_IDLE) break;
	}
	if (r1 != R1_IDLE) return _fail(p_owner, SDSS_CMD0, r1, r1 == R1_NONE ? E_CANNOT_FOUND : E_ACCESS_FAILED);

	// CMD8: a version 2 card echoes the check pattern, a version 1 card calls the command illegal.
	r1 = _cmd(p_owner, 8, 0x1AA);
	if (r1 == R1_IDLE) {
		(*p_owner->__act_transfer)(p_owner, NULL, buf, 4);
		if (buf[2] != 0x01 || buf[3] != 0xAA) return _fail(p_owner, SDSS_CMD8, r1, E_WRONG_DATA);
		p_owner->_type = SDST_SDSC_V2;
		arg = 1UL << 30;		// HCS: a high capacity card is welcome
	}
	else if (r1 != R1_NONE && (r1 & R1_ILLEGAL)) {
		p_owner->_type = SDST_SDSC_V1;
		arg = 0;
	}
	else return _fail(p_owner, SDSS_CMD8, r1, E_ACCESS_FAILED);

	start = pif_cumulative_timer1ms;
	do {
		r1 = _cmd(p_owner, ACMD(41), arg);
		if (r1 == 0) break;
	} while (!PIF_CHECK_ELAPSE_TIME_1MS(start, p_owner->init_timeout));
	if (r1 != 0) {
		p_owner->_type = SDST_NONE;
		return _fail(p_owner, SDSS_ACMD41, r1, E_TIMEOUT);
	}

	if (p_owner->_type == SDST_SDSC_V2) {
		r1 = _cmd(p_owner, 58, 0);
		if (r1 != 0) {
			p_owner->_type = SDST_NONE;
			return _fail(p_owner, SDSS_CMD58, r1, E_ACCESS_FAILED);
		}
		(*p_owner->__act_transfer)(p_owner, NULL, buf, 4);
		p_owner->_ocr = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
		if (p_owner->_ocr & (1UL << 30)) p_owner->_type = SDST_SDHC;		// CCS
	}
	if (p_owner->_type != SDST_SDHC) {
		r1 = _cmd(p_owner, 16, PIF_SD_SPI_SECTOR_SIZE);
		if (r1 != 0) {
			p_owner->_type = SDST_NONE;
			return _fail(p_owner, SDSS_CMD16, r1, E_ACCESS_FAILED);
		}
	}

	r1 = _cmd(p_owner, 9, 0);
	if (r1 != 0 || !_readData(p_owner, buf, 16)) {
		p_owner->_type = SDST_NONE;
		return _fail(p_owner, SDSS_CSD, r1, r1 ? E_ACCESS_FAILED : E_TIMEOUT);
	}
	p_owner->_sectors = _sectorsFromCsd(buf);
	_deselect(p_owner);

	if (p_owner->__act_set_clock) (*p_owner->__act_set_clock)(p_owner, TRUE);
	return TRUE;
}

static uint32_t _address(PifSdSpi *p_owner, uint32_t sector)
{
	return p_owner->_type == SDST_SDHC ? sector : sector * PIF_SD_SPI_SECTOR_SIZE;
}

static BOOL _check(PifSdSpi *p_owner, uint32_t sector, const void *p_data, uint32_t count)
{
	if (!p_owner || !p_data || !count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_type == SDST_NONE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (sector >= p_owner->_sectors || count > p_owner->_sectors - sector) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

BOOL pifSdSpi_Read(PifSdSpi *p_owner, uint32_t sector, uint8_t *p_data, uint32_t count)
{
	uint8_t r1;
	uint32_t i;

	if (!_check(p_owner, sector, p_data, count)) return FALSE;

	if (count == 1) {
		r1 = _cmd(p_owner, 17, _address(p_owner, sector));
		if (r1 != 0) return _fail(p_owner, SDSS_READ, r1, E_ACCESS_FAILED);
		if (!_readData(p_owner, p_data, PIF_SD_SPI_SECTOR_SIZE)) return _fail(p_owner, SDSS_READ, r1, E_TIMEOUT);
		_deselect(p_owner);
		return TRUE;
	}

	r1 = _cmd(p_owner, 18, _address(p_owner, sector));
	if (r1 != 0) return _fail(p_owner, SDSS_READ, r1, E_ACCESS_FAILED);
	for (i = 0; i < count; i++) {
		if (!_readData(p_owner, p_data + i * PIF_SD_SPI_SECTOR_SIZE, PIF_SD_SPI_SECTOR_SIZE)) {
			_cmd(p_owner, 12, 0);
			return _fail(p_owner, SDSS_READ, r1, E_TIMEOUT);
		}
	}
	r1 = _cmd(p_owner, 12, 0);
	if (!_waitReady(p_owner, p_owner->write_timeout)) return _fail(p_owner, SDSS_READ, r1, E_TIMEOUT);
	_deselect(p_owner);
	return TRUE;
}

/**
 * @fn _sendData
 * @brief Sends one sector after its token and checks the data response. The card is busy afterwards.
 */
static BOOL _sendData(PifSdSpi *p_owner, uint8_t token, const uint8_t *p_data)
{
	uint8_t crc[2] = { 0xFF, 0xFF };

	_xfer(p_owner, 0xFF);
	_xfer(p_owner, token);
	(*p_owner->__act_transfer)(p_owner, p_data, NULL, PIF_SD_SPI_SECTOR_SIZE);
	(*p_owner->__act_transfer)(p_owner, crc, NULL, 2);
	return (_xfer(p_owner, 0xFF) & 0x1F) == DATA_ACCEPTED;
}

BOOL pifSdSpi_Write(PifSdSpi *p_owner, uint32_t sector, const uint8_t *p_data, uint32_t count)
{
	uint8_t r1;
	uint32_t i;

	if (!_check(p_owner, sector, p_data, count)) return FALSE;

	if (count == 1) {
		r1 = _cmd(p_owner, 24, _address(p_owner, sector));
		if (r1 != 0) return _fail(p_owner, SDSS_WRITE, r1, E_ACCESS_FAILED);
		if (!_sendData(p_owner, TOKEN_START, p_data)) return _fail(p_owner, SDSS_WRITE, r1, E_WRONG_DATA);
		if (!_waitReady(p_owner, p_owner->write_timeout)) return _fail(p_owner, SDSS_WRITE, r1, E_TIMEOUT);
		_deselect(p_owner);
		return TRUE;
	}

	r1 = _cmd(p_owner, 25, _address(p_owner, sector));
	if (r1 != 0) return _fail(p_owner, SDSS_WRITE, r1, E_ACCESS_FAILED);
	for (i = 0; i < count; i++) {
		if (!_sendData(p_owner, TOKEN_MULTI_WRITE, p_data + i * PIF_SD_SPI_SECTOR_SIZE) ||
				!_waitReady(p_owner, p_owner->write_timeout)) {
			_xfer(p_owner, TOKEN_STOP);
			_waitReady(p_owner, p_owner->write_timeout);
			return _fail(p_owner, SDSS_WRITE, r1, E_WRONG_DATA);
		}
	}
	_xfer(p_owner, TOKEN_STOP);
	_xfer(p_owner, 0xFF);
	if (!_waitReady(p_owner, p_owner->write_timeout)) return _fail(p_owner, SDSS_WRITE, r1, E_TIMEOUT);
	_deselect(p_owner);
	return TRUE;
}
