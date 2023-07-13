#include "core/pif_spi.h"
#include "core/pif_task.h"


BOOL pifSpiPort_Init(PifSpiPort* p_owner, PifId id, uint8_t device_count, uint16_t max_transfer_size)
{
	if (!p_owner || !device_count || !max_transfer_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSpiPort));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__max_transfer_size = max_transfer_size;
    if (!pifFixList_Init(&p_owner->__devices, sizeof(PifSpiDevice), device_count)) goto fail;
    return TRUE;

fail:
	pifSpiPort_Clear(p_owner);
	return FALSE;
}

void pifSpiPort_Clear(PifSpiPort* p_owner)
{
	pifFixList_Clear(&p_owner->__devices, NULL);
}

PifSpiDevice* pifSpiPort_AddDevice(PifSpiPort* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	PifSpiDevice* p_device = (PifSpiDevice*)pifFixList_AddFirst(&p_owner->__devices);
    if (!p_device) return FALSE;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_device->_id = id;
    p_device->__p_port = p_owner;
    p_device->timeout = 10;		// 10ms
    return p_device;
}

void pifSpiPort_RemoveDevice(PifSpiPort* p_owner, PifSpiDevice* p_device)
{
	if (p_device) {
		pifFixList_Remove(&p_owner->__devices, p_device);
		p_device = NULL;
	}
}

BOOL pifSpiDevice_Transfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
	PifSpiPort* p_port = p_owner->__p_port;

	if (!p_port->act_transfer) return FALSE;

	return (*p_port->act_transfer)(p_port->_id, p_write, p_read, size);
}

BOOL pifSpiDevice_Read(PifSpiDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifSpiPort* p_port = p_owner->__p_port;
	uint8_t len;
	size_t ptr, remain;

	if (!p_port->act_read) return FALSE;

	ptr = 0;
	remain = size;
	while (remain) {
		len = remain > p_port->__max_transfer_size ? p_port->__max_transfer_size : remain;
		if (!ptr) {
			if (!(*p_port->act_read)(p_owner->_id, iaddr, isize, p_data, len)) goto fail;
		}
		else {
			if (!(*p_port->act_read)(p_owner->_id, 0UL, 0, p_data + ptr, len)) goto fail;
		}

		ptr += len;
		remain -= len;
	}
	return TRUE;

fail:
	p_port->error_count++;
	pif_error = E_TRANSFER_FAILED;
	return FALSE;
}

BOOL pifSpiDevice_ReadRegByte(PifSpiDevice* p_owner, uint8_t reg, uint8_t* p_data)
{
	return pifSpiDevice_Read(p_owner, reg, 1, p_data, 1);
}

BOOL pifSpiDevice_ReadRegWord(PifSpiDevice* p_owner, uint8_t reg, uint16_t* p_data)
{
	uint8_t tmp[2];

	if (!pifSpiDevice_Read(p_owner, reg, 1, tmp, 2)) return FALSE;
	*p_data = (tmp[0] << 8) + tmp[1];
	return TRUE;
}

BOOL pifSpiDevice_ReadRegBytes(PifSpiDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size)
{
	return pifSpiDevice_Read(p_owner, reg, 1, p_data, size);
}

BOOL pifSpiDevice_ReadRegBit8(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint8_t* p_data)
{
	uint8_t tmp, shift, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (!pifSpiDevice_Read(p_owner, reg, 1, &tmp, 1)) return FALSE;
	*p_data = (tmp >> shift) & mask;
	return TRUE;
}

BOOL pifSpiDevice_ReadRegBit16(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint16_t* p_data)
{
	uint8_t tmp[2], shift;
	uint16_t mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (!pifSpiDevice_Read(p_owner, reg, 1, tmp, 2)) return FALSE;
	*p_data = (((tmp[0] << 8) + tmp[1]) >> shift) & mask;
	return TRUE;
}

BOOL pifSpiDevice_Write(PifSpiDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifSpiPort* p_port = p_owner->__p_port;
	uint8_t len;
	size_t ptr, remain;

	if (!p_port->act_write) return FALSE;

	ptr = 0;
	remain = size;
	while (remain) {
		len = remain > p_port->__max_transfer_size ? p_port->__max_transfer_size : remain;
		if (!ptr) {
			if (!(*p_port->act_write)(p_owner->_id, iaddr, isize, p_data, len)) goto fail;
		}
		else {
			if (!(*p_port->act_write)(p_owner->_id, 0UL, 0, p_data + ptr, len)) goto fail;
		}

		ptr += len;
		remain -= len;
	}
	return TRUE;

fail:
	p_port->error_count++;
	pif_error = E_TRANSFER_FAILED;
	return FALSE;
}

BOOL pifSpiDevice_WriteRegByte(PifSpiDevice* p_owner, uint8_t reg, uint8_t data)
{
	return pifSpiDevice_Write(p_owner, reg, 1, &data, 1);
}

BOOL pifSpiDevice_WriteRegWord(PifSpiDevice* p_owner, uint8_t reg, uint16_t data)
{
	uint8_t tmp[2];

	tmp[0] = data >> 8;
	tmp[1] = data & 0xFF;
	if (!pifSpiDevice_Write(p_owner, reg, 1, tmp, 2)) return FALSE;
    return TRUE;
}

BOOL pifSpiDevice_WriteRegBytes(PifSpiDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size)
{
	return pifSpiDevice_Write(p_owner, reg, 1, p_data, size);
}

BOOL pifSpiDevice_WriteRegBit8(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint8_t data)
{
	uint8_t tmp, org, shift, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifSpiDevice_Read(p_owner, reg, 1, &org, 1)) return FALSE;

	if (((org >> shift) & mask) != data) {
		tmp = (org & ~(mask << shift)) | (data << shift);
		if (!pifSpiDevice_Write(p_owner, reg, 1, &tmp, 1)) return FALSE;
	}
    return TRUE;
}

BOOL pifSpiDevice_WriteRegBit16(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint16_t data)
{
	uint8_t tmp[2], shift;
	uint16_t org, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifSpiDevice_ReadRegWord(p_owner, reg, &org)) return FALSE;

	if (((org >> shift) & mask) != data) {
		org = (org & ~(mask << shift)) | (data << shift);
		tmp[0] = org >> 8;
		tmp[1] = org & 0xFF;
		if (!pifSpiDevice_Write(p_owner, reg, 1, tmp, 2)) return FALSE;
	}
    return TRUE;
}
