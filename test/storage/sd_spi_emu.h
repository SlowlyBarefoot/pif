// SPDX-License-Identifier: BSD-3-Clause
// An SD card in SPI mode, in memory, byte by byte as it sees them: commands with their R1 a byte later, the R3 and
// R7 answers, the CSD, single and multiple block reads and writes with their tokens, data responses and busy
// bytes. It answers only while selected, as a card does. Every 64 bytes it advances pif's millisecond clock, so the
// timeouts of pif_sd_spi run out in a test.
#ifndef SD_SPI_EMU_H
#define SD_SPI_EMU_H

#include "storage/pif_sd_spi.h"

#include <stdio.h>
#include <string.h>


#define EMU_SECTORS			2048		// 1MB: the smallest a CSD version 2 can give is 1024 sectors
#define EMU_BUSY_BYTES		4			// busy bytes after a sector is written

typedef enum
{
	EMU_NO_CARD,
	EMU_SDSC_V1,
	EMU_SDSC_V2,
	EMU_SDHC
} EmuCardKind;

typedef enum
{
	EM_CMD,					// waiting for a command
	EM_WRITE_TOKEN,			// CMD24 / CMD25: waiting for a data token
	EM_WRITE_DATA,			// taking 512 bytes and the CRC
	EM_READ_MULTI			// CMD18: sending sectors until CMD12
} EmuMode;

static EmuCardKind s_emu_kind = EMU_SDHC;
static uint8_t s_emu_media[EMU_SECTORS][PIF_SD_SPI_SECTOR_SIZE];
static BOOL s_emu_cs;
static BOOL s_emu_idle = TRUE;
static BOOL s_emu_app;
static int s_emu_acmd41;				// ACMD41 so far
static int s_emu_acmd41_needed = 3;		// how many it takes to leave idle; -1 for never
static BOOL s_emu_reject_write;			// data response "rejected" (CRC error)
static int s_emu_fast;					// clock calls with TRUE
static uint32_t s_emu_bytes;
static int s_emu_cmd_count[64];

static EmuMode s_emu_mode;
static uint8_t s_emu_cmd[6];
static int s_emu_cmd_len;
static uint8_t s_emu_out[600];
static int s_emu_out_head, s_emu_out_len;
static int s_emu_busy;
static BOOL s_emu_multi;
static uint32_t s_emu_sector;
static int s_emu_data_len;
static uint8_t s_emu_data[PIF_SD_SPI_SECTOR_SIZE + 2];


static void _emuReset(EmuCardKind kind)
{
	s_emu_kind = kind;
	s_emu_cs = FALSE;
	s_emu_idle = TRUE;
	s_emu_app = FALSE;
	s_emu_acmd41 = 0;
	s_emu_acmd41_needed = 3;
	s_emu_reject_write = FALSE;
	s_emu_fast = 0;
	s_emu_mode = EM_CMD;
	s_emu_cmd_len = 0;
	s_emu_out_head = s_emu_out_len = 0;
	s_emu_busy = 0;
	memset(s_emu_cmd_count, 0, sizeof(s_emu_cmd_count));
}

static void _emuPush(uint8_t b)
{
	if (s_emu_out_head + s_emu_out_len < (int)sizeof(s_emu_out)) s_emu_out[s_emu_out_head + s_emu_out_len++] = b;
}

static void _emuPushSector(uint32_t sector)
{
	int i;

	s_emu_out_head = s_emu_out_len = 0;
	_emuPush(0xFF);
	_emuPush(0xFE);
	for (i = 0; i < PIF_SD_SPI_SECTOR_SIZE; i++) _emuPush(s_emu_media[sector][i]);
	_emuPush(0x12);		// CRC, not checked
	_emuPush(0x34);
}

static void _emuCsd(uint8_t *p)
{
	memset(p, 0, 16);
	if (s_emu_kind == EMU_SDHC) {
		uint32_t c_size = EMU_SECTORS / 1024 - 1;
		p[0] = 0x40;
		p[7] = (uint8_t)((c_size >> 16) & 0x3F);
		p[8] = (uint8_t)(c_size >> 8);
		p[9] = (uint8_t)c_size;
	}
	else {
		// READ_BL_LEN 9, C_SIZE_MULT 7: (C_SIZE + 1) x 512 sectors
		uint32_t c_size = EMU_SECTORS / 512 - 1;
		p[5] = 9;
		p[6] = (uint8_t)((c_size >> 10) & 0x03);
		p[7] = (uint8_t)(c_size >> 2);
		p[8] = (uint8_t)((c_size & 0x03) << 6);
		p[9] = 0x03;		// C_SIZE_MULT high bits
		p[10] = 0x80;		// C_SIZE_MULT low bit
	}
}

static BOOL _emuSector(uint32_t arg, uint32_t *p_sector)
{
	if (s_emu_kind != EMU_SDHC) {
		if (arg % PIF_SD_SPI_SECTOR_SIZE) return FALSE;
		arg /= PIF_SD_SPI_SECTOR_SIZE;
	}
	if (arg >= EMU_SECTORS) return FALSE;
	*p_sector = arg;
	return TRUE;
}

static void _emuCommand(void)
{
	uint8_t cmd = s_emu_cmd[0] & 0x3F, r1, csd[16];
	uint32_t arg = ((uint32_t)s_emu_cmd[1] << 24) | ((uint32_t)s_emu_cmd[2] << 16) | ((uint32_t)s_emu_cmd[3] << 8) |
			s_emu_cmd[4];
	BOOL app = s_emu_app;
	int i;

	s_emu_app = FALSE;
	s_emu_cmd_count[cmd]++;
	s_emu_out_head = s_emu_out_len = 0;
	_emuPush(0xFF);		// NCR: the answer comes a byte later

	if (cmd == 0) {
		if (s_emu_cmd[5] != 0x95) return;		// bad CRC: no answer
		s_emu_idle = TRUE;
		s_emu_acmd41 = 0;
		_emuPush(0x01);
		return;
	}
	r1 = s_emu_idle ? 0x01 : 0x00;
	switch (app ? 0x80 | cmd : cmd) {
	case 8:
		if (s_emu_kind == EMU_SDSC_V1) { _emuPush(r1 | 0x04); break; }
		if (s_emu_cmd[5] != 0x87) { _emuPush(r1 | 0x08); break; }		// CRC error
		_emuPush(r1);
		_emuPush(0x00); _emuPush(0x00); _emuPush((uint8_t)(arg >> 8) & 0x0F); _emuPush((uint8_t)arg);
		break;
	case 55:
		s_emu_app = TRUE;
		_emuPush(r1);
		break;
	case 0x80 | 41:
		// A high capacity card stays idle unless the host says it takes high capacity.
		if (s_emu_kind == EMU_SDHC && !(arg & (1UL << 30))) { _emuPush(r1); break; }
		s_emu_acmd41++;
		if (s_emu_acmd41_needed >= 0 && s_emu_acmd41 >= s_emu_acmd41_needed) s_emu_idle = FALSE;
		_emuPush(s_emu_idle ? 0x01 : 0x00);
		break;
	case 58:
		_emuPush(r1);
		_emuPush((s_emu_idle ? 0x00 : 0x80) | (s_emu_kind == EMU_SDHC ? 0x40 : 0x00));
		_emuPush(0xFF); _emuPush(0x80); _emuPush(0x00);
		break;
	case 16:
		_emuPush(arg == PIF_SD_SPI_SECTOR_SIZE ? r1 : (r1 | 0x40));
		break;
	case 9:
		_emuPush(r1);
		_emuPush(0xFF);
		_emuPush(0xFE);
		_emuCsd(csd);
		for (i = 0; i < 16; i++) _emuPush(csd[i]);
		_emuPush(0x00); _emuPush(0x00);
		break;
	case 17:
		if (s_emu_idle || !_emuSector(arg, &s_emu_sector)) { _emuPush(r1 | 0x40); break; }
		_emuPush(0x00);
		_emuPush(0xFF);
		_emuPush(0xFE);
		for (i = 0; i < PIF_SD_SPI_SECTOR_SIZE; i++) _emuPush(s_emu_media[s_emu_sector][i]);
		_emuPush(0x00); _emuPush(0x00);
		break;
	case 18:
		if (s_emu_idle || !_emuSector(arg, &s_emu_sector)) { _emuPush(r1 | 0x40); break; }
		_emuPush(0x00);
		s_emu_mode = EM_READ_MULTI;
		break;
	case 24:
	case 25:
		if (s_emu_idle || !_emuSector(arg, &s_emu_sector)) { _emuPush(r1 | 0x40); break; }
		_emuPush(0x00);
		s_emu_multi = cmd == 25;
		s_emu_mode = EM_WRITE_TOKEN;
		break;
	case 12:
		_emuPush(0x00);
		break;
	default:
		_emuPush(r1 | 0x04);		// illegal command
		break;
	}
}

static uint8_t _emuByte(uint8_t in)
{
	uint8_t out = 0xFF;

	if (++s_emu_bytes % 64 == 0) pif_sigTimer1ms();
	if (s_emu_kind == EMU_NO_CARD || !s_emu_cs) return 0xFF;

	// What goes out on this byte.
	if (s_emu_out_len) {
		out = s_emu_out[s_emu_out_head++];
		s_emu_out_len--;
	}
	else if (s_emu_busy) {
		out = 0x00;
		s_emu_busy--;
	}
	else if (s_emu_mode == EM_READ_MULTI) {
		_emuPushSector(s_emu_sector);
		s_emu_sector = (s_emu_sector + 1) % EMU_SECTORS;
		out = s_emu_out[s_emu_out_head++];
		s_emu_out_len--;
	}

	// What comes in.
	switch (s_emu_mode) {
	case EM_WRITE_TOKEN:
		if (in == 0xFE || (s_emu_multi && in == 0xFC)) {
			s_emu_mode = EM_WRITE_DATA;
			s_emu_data_len = 0;
		}
		else if (s_emu_multi && in == 0xFD) {
			s_emu_busy = EMU_BUSY_BYTES;
			s_emu_mode = EM_CMD;
		}
		break;

	case EM_WRITE_DATA:
		s_emu_data[s_emu_data_len++] = in;
		if (s_emu_data_len == PIF_SD_SPI_SECTOR_SIZE + 2) {
			s_emu_out_head = s_emu_out_len = 0;
			if (s_emu_reject_write) {
				_emuPush(0x0B);			// rejected, CRC error
				s_emu_mode = s_emu_multi ? EM_WRITE_TOKEN : EM_CMD;
				break;
			}
			memcpy(s_emu_media[s_emu_sector], s_emu_data, PIF_SD_SPI_SECTOR_SIZE);
			_emuPush(0xE5);				// accepted (xxx0 0101)
			s_emu_busy = EMU_BUSY_BYTES;
			if (s_emu_multi) {
				s_emu_sector++;
				s_emu_mode = EM_WRITE_TOKEN;
			}
			else s_emu_mode = EM_CMD;
		}
		break;

	default:		// EM_CMD, EM_READ_MULTI
		if (s_emu_cmd_len) {
			s_emu_cmd[s_emu_cmd_len++] = in;
			if (s_emu_cmd_len == 6) {
				s_emu_cmd_len = 0;
				if (s_emu_mode == EM_READ_MULTI) {
					if ((s_emu_cmd[0] & 0x3F) != 12) break;
					s_emu_mode = EM_CMD;
					s_emu_cmd_count[12]++;
					s_emu_out_head = s_emu_out_len = 0;
					_emuPush(0xFF);		// stuff byte
					_emuPush(0x00);
					s_emu_busy = 2;
					break;
				}
				_emuCommand();
			}
		}
		else if ((in & 0xC0) == 0x40) {
			s_emu_cmd[0] = in;
			s_emu_cmd_len = 1;
		}
		break;
	}
	return out;
}

static void _emuSelect(PifSdSpi *p_owner, BOOL select)
{
	(void)p_owner;
	s_emu_cs = select;
	if (!select) s_emu_cmd_len = 0;
}

static void _emuTransfer(PifSdSpi *p_owner, const uint8_t *p_write, uint8_t *p_read, size_t size)
{
	size_t i;
	uint8_t in;

	(void)p_owner;
	for (i = 0; i < size; i++) {
		in = _emuByte(p_write ? p_write[i] : 0xFF);
		if (p_read) p_read[i] = in;
	}
}

static void _emuSetClock(PifSdSpi *p_owner, BOOL fast)
{
	(void)p_owner;
	if (fast) s_emu_fast++;
}

#endif	// SD_SPI_EMU_H
