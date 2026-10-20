// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_device_reg.h"


void pifDeviceReg_Init(PifDeviceReg *p_owner, PifId id, void *p_client, const PifDeviceRegOps *p_ops)
{
	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_p_client = p_client;
	p_owner->__p_ops = p_ops;
	p_owner->_state = DRS_IDLE;
	// Left at 0, a blocking transfer would time out as soon as the 1ms clock ticked while it waited.
	p_owner->timeout = 10;
}

BOOL pifDeviceReg_Read(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return (*p_owner->__p_ops->read)(p_owner, iaddr, isize, p_data, size);
}

BOOL pifDeviceReg_Write(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return (*p_owner->__p_ops->write)(p_owner, iaddr, isize, p_data, size);
}

BOOL pifDeviceReg_ReadRegByte(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data)
{
	return pifDeviceReg_Read(p_owner, reg, 1, p_data, 1);
}

BOOL pifDeviceReg_ReadRegWord(PifDeviceReg *p_owner, uint8_t reg, uint16_t *p_data)
{
	uint8_t tmp[2];

	if (!pifDeviceReg_Read(p_owner, reg, 1, tmp, 2)) return FALSE;

	*p_data = (tmp[0] << 8) + tmp[1];
	return TRUE;
}

BOOL pifDeviceReg_ReadRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size)
{
	return pifDeviceReg_Read(p_owner, reg, 1, p_data, size);
}

BOOL pifDeviceReg_ReadRegBit8(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint8_t *p_data)
{
	uint8_t tmp;

	if (!pifDeviceReg_Read(p_owner, reg, 1, &tmp, 1)) return FALSE;

	*p_data = tmp & mask;
	return TRUE;
}

BOOL pifDeviceReg_ReadRegBit16(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint16_t *p_data)
{
	uint16_t tmp;

	if (!pifDeviceReg_ReadRegWord(p_owner, reg, &tmp)) return FALSE;

	*p_data = tmp & mask;
	return TRUE;
}

BOOL pifDeviceReg_WriteRegByte(PifDeviceReg *p_owner, uint8_t reg, uint8_t data)
{
	return pifDeviceReg_Write(p_owner, reg, 1, &data, 1);
}

BOOL pifDeviceReg_WriteRegWord(PifDeviceReg *p_owner, uint8_t reg, uint16_t data)
{
	uint8_t tmp[2];

	tmp[0] = data >> 8;
	tmp[1] = data & 0xFF;
	return pifDeviceReg_Write(p_owner, reg, 1, tmp, 2);
}

BOOL pifDeviceReg_WriteRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size)
{
	return pifDeviceReg_Write(p_owner, reg, 1, p_data, size);
}

BOOL pifDeviceReg_WriteRegBit8(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint8_t data)
{
	uint8_t tmp, org;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	if (!pifDeviceReg_Read(p_owner, reg, 1, &org, 1)) return FALSE;

	if ((org & mask) != data) {
		tmp = (org & ~mask) | data;
		if (!pifDeviceReg_Write(p_owner, reg, 1, &tmp, 1)) return FALSE;
	}
	return TRUE;
}

BOOL pifDeviceReg_WriteRegBit16(PifDeviceReg *p_owner, uint8_t reg, PifRegMask mask, uint16_t data)
{
	uint16_t org;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	if (!pifDeviceReg_ReadRegWord(p_owner, reg, &org)) return FALSE;

	if ((org & mask) != data) {
		SET_BIT_FILED(org, mask, data);
		if (!pifDeviceReg_WriteRegWord(p_owner, reg, org)) return FALSE;
	}
	return TRUE;
}

BOOL pifDeviceReg_StartRead(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return (*p_owner->__p_ops->start_read)(p_owner, iaddr, isize, p_data, size);
}

BOOL pifDeviceReg_StartReadRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size)
{
	return pifDeviceReg_StartRead(p_owner, reg, 1, p_data, size);
}

BOOL pifDeviceReg_StartWrite(PifDeviceReg *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return (*p_owner->__p_ops->start_write)(p_owner, iaddr, isize, p_data, size);
}

BOOL pifDeviceReg_StartWriteRegBytes(PifDeviceReg *p_owner, uint8_t reg, uint8_t *p_data, size_t size)
{
	return pifDeviceReg_StartWrite(p_owner, reg, 1, p_data, size);
}

PifDeviceRegState pifDeviceReg_CheckTransfer(PifDeviceReg *p_owner)
{
	if (!p_owner) return DRS_IDLE;
	return (*p_owner->__p_ops->check_transfer)(p_owner);
}

BOOL pifDeviceReg_IsBusy(PifDeviceReg *p_owner)
{
	if (!p_owner) return FALSE;
	return (*p_owner->__p_ops->is_busy)(p_owner);
}
