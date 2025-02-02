#include "osd/pif_max7456.h"


BOOL pifMax7456_Init(PifMax7456* p_owner, PifId id, PifSpiPort* p_port, void *p_client)
{
	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMax7456));
	p_owner->__previous_invert = 0xFF;
	pifMax7456_InitBrightness(p_owner);

	p_owner->_p_spi = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, p_client);
    if (!p_owner->_p_spi) return FALSE;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

void pifMax7456_Clear(PifMax7456* p_owner)
{
	if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
	}
}

BOOL pifMax7456_WriteNvm(PifMax7456* p_owner, uint8_t char_address, const uint8_t *font_data)
{
    uint8_t data;

    if (!p_owner->detected) {
        return FALSE;
    }

    // Block pending completion of any prior SPI access
    pifSpiDevice_Wait(p_owner->_p_spi);

    // disable display
    p_owner->_font_is_loading = TRUE;
    pifSpiDevice_WriteRegByte(p_owner->_p_spi, MAX7456_REG_W_VM0, 0);

    pifSpiDevice_WriteRegByte(p_owner->_p_spi, MAX7456_REG_W_CMAH, char_address); // set start address high

    for (int x = 0; x < 54; x++) {
        pifSpiDevice_WriteRegByte(p_owner->_p_spi, MAX7456_REG_W_CMAL, x);    //set start address low
        pifSpiDevice_WriteRegByte(p_owner->_p_spi, MAX7456_REG_W_CMDI, font_data[x]);
    }

    // Transfer 54 bytes from shadow ram to NVM
    pifSpiDevice_WriteRegByte(p_owner->_p_spi, MAX7456_REG_W_CMM, 0xA0);

    // Wait until bit 5 in the status register returns to 0 (12ms)
    do {
        pifSpiDevice_ReadRegByte(p_owner->_p_spi, MAX7456_REG_R_STAT, &data);
    } while ((data & MAX7456_CHAR_MEM_STATUS_MASK) != 0x00);

    return TRUE;
}

BOOL pifMax7456_Invert(PifMax7456* p_owner, BOOL invert)
{
    if (invert) {
        p_owner->_display_memory_mode |= MAX7456_INVERT_BIT_INVERT;
    } else {
        p_owner->_display_memory_mode &= ~MAX7456_INVERT_BIT_INVERT;
    }

    if (p_owner->_display_memory_mode != p_owner->__previous_invert) {
        p_owner->__previous_invert = p_owner->_display_memory_mode;
        pifSpiDevice_WriteRegByte(p_owner->_p_spi, MAX7456_REG_W_DMM, p_owner->_display_memory_mode);
		return TRUE;
    }
	return FALSE;
}

void pifMax7456_InitBrightness(PifMax7456* p_owner)
{
    p_owner->__previous_brightness = 0xFF;
}

void pifMax7456_Brightness(PifMax7456* p_owner, uint8_t black, uint8_t white)
{
    const uint8_t reg = (black << 2) | (3 - white);
    uint8_t buf[32];

    if (reg != p_owner->__previous_brightness) {
        p_owner->__previous_brightness = reg;
        for (int i = MAX7456_REG_W_RB0, j = 0; i <= MAX7456_REG_W_RB15; i++) {
            buf[j++] = i;
            buf[j++] = reg;
        }
        pifSpiDevice_Transfer(p_owner->_p_spi, buf, NULL, sizeof(buf));
    }
}
