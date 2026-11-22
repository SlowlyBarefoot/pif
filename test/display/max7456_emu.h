// SPDX-License-Identifier: BSD-3-Clause
// A MAX7456 in memory behind a PifSpiPort: the registers the driver writes, the display memory with its
// auto-increment run and the escape that ends it, the shadow RAM and the character NVM behind it, and the bits the
// chip clears itself when a software reset, a display memory clear or an NVM write is through.
#ifndef MAX7456_EMU_H
#define MAX7456_EMU_H

#include "display/osd/pif_max7456.h"

#include <stdio.h>
#include <string.h>


#define EMU_DISPLAY_SIZE	480				// 30 columns by 16 rows
#define EMU_CHARS			256				// Characters of the font in character memory
#define EMU_OSDM_POWER_ON	0x1B			// What OSDM holds, which is what pifMax7456_Detect() looks for
#define EMU_BUSY_READS		2				// Reads that show an operation of the chip still running


static PifSpiPort s_emu_port;

static uint8_t s_emu_vm0, s_emu_vm1, s_emu_dmm, s_emu_osdm;
static uint8_t s_emu_rb[MAX7456_REG_W_RB15 - MAX7456_REG_W_RB0 + 1];
static uint8_t s_emu_stat;					// The signal bits of STAT, as a camera on the input would leave them
static uint16_t s_emu_dma;					// Where DMAH and DMAL point in the display memory
static BOOL s_emu_attr_byte;				// The byte select bit of DMAH: the attribute, not the character
static BOOL s_emu_auto;						// A run of auto-increment mode is on
static uint8_t s_emu_display[EMU_DISPLAY_SIZE];
static uint8_t s_emu_cmah, s_emu_cmal;
static uint8_t s_emu_shadow[MAX7456_CHAR_DATA_SIZE];
static uint8_t s_emu_nvm[EMU_CHARS][MAX7456_CHAR_DATA_SIZE];
static uint8_t s_emu_vm0_on_nvm;			// VM0 as it stood when the NVM write was asked for

static int s_emu_reset_busy;				// Reads of VM0 that still show the software reset running
static int s_emu_clear_busy;				// Reads of DMM that still show the display memory being cleared
static int s_emu_nvm_busy;					// Reads of STAT that still show the character memory unavailable
static BOOL s_emu_stuck;					// No such bit ever clears, and every read of one takes a millisecond
static BOOL s_emu_absent;					// Nothing answers: the bus reads as ones
static int s_emu_fail_in;					// Accesses that go through before they all fail, -1 for none
static int s_emu_transfers;					// Transfers made under one chip select
static int s_emu_writes;					// Registers written


static void _emuReset(void)
{
    s_emu_vm0 = s_emu_vm1 = s_emu_dmm = 0;
    s_emu_osdm = EMU_OSDM_POWER_ON;
    memset(s_emu_rb, 0, sizeof(s_emu_rb));
    s_emu_stat = 0;
    s_emu_dma = 0;
    s_emu_attr_byte = FALSE;
    s_emu_auto = FALSE;
    memset(s_emu_display, 0, sizeof(s_emu_display));
    s_emu_cmah = s_emu_cmal = 0;
    memset(s_emu_shadow, 0, sizeof(s_emu_shadow));
    memset(s_emu_nvm, 0, sizeof(s_emu_nvm));
    s_emu_vm0_on_nvm = 0xFF;
    s_emu_reset_busy = s_emu_clear_busy = s_emu_nvm_busy = 0;
    s_emu_stuck = FALSE;
    s_emu_absent = FALSE;
    s_emu_fail_in = -1;
    s_emu_transfers = s_emu_writes = 0;
}

// One register written, whether it came on its own or as a pair of an SPI transfer.
static void _emuRegWrite(uint8_t reg, uint8_t val)
{
    s_emu_writes++;
    switch (reg) {
    case MAX7456_REG_W_VM0:
        s_emu_vm0 = val;
        if (val & MAX7456_SOFTWARE_RESET_BIT_MASK) {
            // Every register goes back to its power-on value, the display memory with them, and the bit stays
            // up until the chip is through: the reads below put it there.
            s_emu_vm0 = s_emu_vm1 = s_emu_dmm = 0;
            memset(s_emu_rb, 0, sizeof(s_emu_rb));
            memset(s_emu_display, 0, sizeof(s_emu_display));
            s_emu_auto = FALSE;
            s_emu_reset_busy = EMU_BUSY_READS;
        }
        break;

    case MAX7456_REG_W_VM1:
        s_emu_vm1 = val;
        break;

    case MAX7456_REG_W_DMM:
        s_emu_dmm = val;
        s_emu_auto = (val & MAX7456_AUTO_INCR_MODE_MASK) != 0;
        if (val & MAX7456_CLEAR_DISP_MEM_MASK) {
            memset(s_emu_display, 0, sizeof(s_emu_display));
            s_emu_clear_busy = EMU_BUSY_READS;
        }
        break;

    case MAX7456_REG_W_DMAH:
        s_emu_dma = (uint16_t)((s_emu_dma & 0xFF) |
                ((val & MAX7456_DISP_MEM_ADDR_BIT_8_MASK) << 8));
        s_emu_attr_byte = (val & MAX7456_BYTE_SELECT_BIT_MASK) != 0;
        break;

    case MAX7456_REG_W_DMAL:
        s_emu_dma = (uint16_t)((s_emu_dma & 0x100) | val);
        break;

    case MAX7456_REG_W_DMDI:
        if (s_emu_auto && val == MAX7456_ESCAPE) {
            s_emu_auto = FALSE;
            s_emu_dmm &= (uint8_t)~MAX7456_AUTO_INCR_MODE_MASK;
            break;
        }
        if (!s_emu_attr_byte && s_emu_dma < EMU_DISPLAY_SIZE) s_emu_display[s_emu_dma] = val;
        if (s_emu_auto) s_emu_dma++;
        break;

    case MAX7456_REG_W_CMAH:
        s_emu_cmah = val;
        break;

    case MAX7456_REG_W_CMAL:
        s_emu_cmal = val;
        break;

    case MAX7456_REG_W_CMDI:
        if (s_emu_cmal < MAX7456_CHAR_DATA_SIZE) s_emu_shadow[s_emu_cmal] = val;
        break;

    case MAX7456_REG_W_CMM:
        if (val == MAX7456_CMM_WRITE_NVM) {
            memcpy(s_emu_nvm[s_emu_cmah], s_emu_shadow, MAX7456_CHAR_DATA_SIZE);
            s_emu_vm0_on_nvm = s_emu_vm0;
            s_emu_nvm_busy = EMU_BUSY_READS;
        }
        break;

    default:
        if (reg >= MAX7456_REG_W_RB0 && reg <= MAX7456_REG_W_RB15) {
            s_emu_rb[reg - MAX7456_REG_W_RB0] = val;
        }
        break;
    }
}

// A bit the chip clears when it is through with something: it shows for a few reads, or for ever while
// s_emu_stuck, where each read is given a millisecond so that the driver reaches its timeout.
static BOOL _emuBusy(int* p_reads)
{
    if (s_emu_stuck) {
        pif_cumulative_timer1ms++;
        return TRUE;
    }
    if (*p_reads <= 0) return FALSE;
    (*p_reads)--;
    return TRUE;
}

static uint8_t _emuRegRead(uint8_t reg)
{
    switch (reg) {
    case MAX7456_REG_R_VM0:
        return (uint8_t)(s_emu_vm0 |
                (_emuBusy(&s_emu_reset_busy) ? MAX7456_SOFTWARE_RESET_BIT_MASK : 0));

    case MAX7456_REG_R_VM1:
        return s_emu_vm1;

    case MAX7456_REG_R_DMM:
        return (uint8_t)((s_emu_dmm & ~MAX7456_CLEAR_DISP_MEM_MASK) |
                (_emuBusy(&s_emu_clear_busy) ? MAX7456_CLEAR_DISP_MEM_MASK : 0));

    case MAX7456_REG_R_OSDM:
        return s_emu_osdm;

    case MAX7456_REG_R_STAT:
        return (uint8_t)(s_emu_stat | (_emuBusy(&s_emu_nvm_busy) ? MAX7456_CHAR_MEM_STATUS_MASK : 0));

    case MAX7456_REG_R_DMDO:
        return s_emu_dma < EMU_DISPLAY_SIZE ? s_emu_display[s_emu_dma] : 0;

    case MAX7456_REG_R_CMDO:
        return s_emu_cmal < MAX7456_CHAR_DATA_SIZE ? s_emu_shadow[s_emu_cmal] : 0;

    default:
        if (reg >= MAX7456_REG_R_RB0 && reg <= MAX7456_REG_R_RB15) {
            return s_emu_rb[reg - MAX7456_REG_R_RB0];
        }
        return 0;
    }
}

// Accesses go through until s_emu_fail_in of them have, and fail from then on.
static BOOL _emuAccess(void)
{
    if (s_emu_fail_in < 0) return TRUE;
    if (s_emu_fail_in == 0) return FALSE;
    s_emu_fail_in--;
    return TRUE;
}

static BOOL _emuRead(PifSpiDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    (void)p_owner;

    if (!_emuAccess()) return FALSE;
    if (isize != 1 || size != 1) return FALSE;
    p_data[0] = s_emu_absent ? 0xFF : _emuRegRead((uint8_t)iaddr);
    return TRUE;
}

static BOOL _emuWrite(PifSpiDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    (void)p_owner;

    if (!_emuAccess()) return FALSE;
    if (isize != 1 || size != 1) return FALSE;
    if (!s_emu_absent) _emuRegWrite((uint8_t)iaddr, p_data[0]);
    return TRUE;
}

// What the chip sees of a transfer under one chip select: pairs of a register address and its value.
static void _emuTransfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
    size_t i;

    (void)p_owner;
    (void)p_read;

    s_emu_transfers++;
    if (!p_write || s_emu_absent) return;
    for (i = 0; i + 1 < size; i += 2) _emuRegWrite(p_write[i], p_write[i + 1]);
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

#endif  // MAX7456_EMU_H
