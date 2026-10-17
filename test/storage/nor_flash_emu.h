// SPDX-License-Identifier: BSD-3-Clause
// An SPI NOR flash in memory behind a PifSpiPort, with the JEDEC commands pif_nor_flash sends: programming that only
// clears bits and wraps within its page, a write enable latch that every program and erase needs and clears, and a
// busy bit that stays up for a few reads of the status register.
#ifndef NOR_FLASH_EMU_H
#define NOR_FLASH_EMU_H

#include "communication/pif_spi.h"

#include <stdio.h>


#define EMU_CAPACITY		0x10000UL		// 64KB, as the JEDEC ID below says
#define EMU_PAGE			256
#define EMU_SECTOR			4096

static PifSpiPort s_emu_port;
static uint8_t s_emu_media[EMU_CAPACITY];
static uint8_t s_emu_jedec[3] = { 0xEF, 0x40, 0x10 };
static BOOL s_emu_wel;
static int s_emu_busy;					// Status reads left that show the device busy
static int s_emu_busy_reads = 2;		// What a program or erase leaves it at
static int s_emu_program_count, s_emu_erase_count;
static int s_emu_cut_after = -1;		// Programs that go through before the power is cut, -1 for never
static BOOL s_emu_power_cut;			// The power was cut: nothing more is programmed or erased
static BOOL s_emu_crossed_page;			// A program went past the end of its page
static BOOL s_emu_ignored;				// A program or erase came without the write enable, or while busy


static void _emuReset(void)
{
    memset(s_emu_media, 0xFF, sizeof(s_emu_media));
    s_emu_wel = FALSE;
    s_emu_busy = 0;
    s_emu_program_count = s_emu_erase_count = 0;
    s_emu_cut_after = -1;
    s_emu_power_cut = FALSE;
    s_emu_crossed_page = FALSE;
    s_emu_ignored = FALSE;
}

static BOOL _emuMayChange(void)
{
    if (s_emu_busy || !s_emu_wel) {
        s_emu_ignored = TRUE;
        return FALSE;
    }
    s_emu_wel = FALSE;
    return !s_emu_power_cut;
}

static void _emuTransfer(PifSpiDevice *p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
    uint32_t addr;

    (void)p_owner;
    (void)p_read;

    if (!p_write || !size) return;
    switch (p_write[0]) {
    case 0x06:
        if (!s_emu_busy) s_emu_wel = TRUE;
        break;

    case 0x20:
        if (size != 4 || !_emuMayChange()) break;
        addr = ((uint32_t)p_write[1] << 16 | p_write[2] << 8 | p_write[3]) % EMU_CAPACITY;
        memset(s_emu_media + addr / EMU_SECTOR * EMU_SECTOR, 0xFF, EMU_SECTOR);
        s_emu_erase_count++;
        s_emu_busy = s_emu_busy_reads;
        break;

    case 0xC7:
        if (!_emuMayChange()) break;
        memset(s_emu_media, 0xFF, sizeof(s_emu_media));
        s_emu_erase_count++;
        s_emu_busy = s_emu_busy_reads;
        break;
    }
}

static BOOL _emuRead(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    uint32_t addr;
    size_t i;

    (void)p_owner;

    if (isize == 1 && iaddr == 0x9F) {
        for (i = 0; i < size; i++) p_data[i] = i < 3 ? s_emu_jedec[i] : 0;
        return TRUE;
    }
    if (isize == 1 && iaddr == 0x05) {
        for (i = 0; i < size; i++) p_data[i] = (s_emu_busy ? 0x01 : 0) | (s_emu_wel ? 0x02 : 0);
        if (s_emu_busy) s_emu_busy--;
        return TRUE;
    }
    if (isize == 4 && (iaddr >> 24) == 0x03) {
        // Ignored while busy: the bus reads as ones.
        if (s_emu_busy) {
            memset(p_data, 0xFF, size);
            s_emu_ignored = TRUE;
            return TRUE;
        }
        addr = iaddr & 0xFFFFFF;
        for (i = 0; i < size; i++) p_data[i] = s_emu_media[(addr + i) % EMU_CAPACITY];
        return TRUE;
    }
    return FALSE;
}

static BOOL _emuWrite(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    uint32_t addr, page;
    size_t i;

    (void)p_owner;

    if (isize != 4 || (iaddr >> 24) != 0x02) return FALSE;
    if (!_emuMayChange()) return TRUE;
    if (s_emu_cut_after >= 0 && s_emu_program_count >= s_emu_cut_after) {
        s_emu_power_cut = TRUE;
        return TRUE;
    }

    addr = iaddr & 0xFFFFFF;
    page = addr / EMU_PAGE * EMU_PAGE;
    if (addr % EMU_PAGE + size > EMU_PAGE) s_emu_crossed_page = TRUE;
    for (i = 0; i < size; i++) {
        s_emu_media[(page + (addr + i) % EMU_PAGE) % EMU_CAPACITY] &= p_data[i];
    }
    s_emu_program_count++;
    s_emu_busy = s_emu_busy_reads;
    return TRUE;
}

static BOOL _emuInit(void)
{
    pifSpiPort_Clear(&s_emu_port);
    if (!pifSpiPort_Init(&s_emu_port, PIF_ID_AUTO, 2)) return FALSE;
    s_emu_port.act_transfer = _emuTransfer;
    s_emu_port.act_read = _emuRead;
    s_emu_port.act_write = _emuWrite;
    _emuReset();
    return TRUE;
}

#endif  // NOR_FLASH_EMU_H
