#include "communication/pif_spi.h"
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
    if (!pifObjArray_Init(&p_owner->__devices, sizeof(PifSpiDevice), device_count, NULL)) goto fail;
    return TRUE;

fail:
	pifSpiPort_Clear(p_owner);
	return FALSE;
}

void pifSpiPort_Clear(PifSpiPort* p_owner)
{
	pifObjArray_Clear(&p_owner->__devices);
}

PifSpiDevice* pifSpiPort_AddDevice(PifSpiPort* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	PifObjArrayIterator it = pifObjArray_Add(&p_owner->__devices);
    if (!it) return FALSE;

    PifSpiDevice* p_device = (PifSpiDevice*)it->data;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_device->_id = id;
    p_device->_p_port = p_owner;
    p_device->timeout = 10;		// 10ms
    return p_device;
}

void pifSpiPort_RemoveDevice(PifSpiPort* p_owner, PifSpiDevice* p_device)
{
	if (p_device) {
		pifObjArray_Remove(&p_owner->__devices, p_device);
		p_device = NULL;
	}
}

PifSpiDevice* pifSpiPort_TemporaryDevice(PifSpiPort* p_owner)
{
	static PifSpiDevice device;

	device._p_port = p_owner;
	return &device;
}

BOOL pifSpiDevice_Transfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
	PifSpiPort* p_port = p_owner->_p_port;

	if (!p_port->act_transfer) return FALSE;

	(*p_port->act_transfer)(p_owner, p_write, p_read, size);
	return TRUE;
}

BOOL pifSpiDevice_Read(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifSpiDevice* p_device = (PifSpiDevice*)p_owner;
	PifSpiPort* p_port = p_device->_p_port;
	uint8_t len;
	size_t ptr, remain;

	if (!p_port->act_read) return FALSE;

	ptr = 0;
	remain = size;
	while (remain) {
		len = remain > p_port->__max_transfer_size ? p_port->__max_transfer_size : remain;
		if (!ptr) {
			if (!(*p_port->act_read)(p_owner, iaddr, isize, p_data, len)) goto fail;
		}
		else {
			if (!(*p_port->act_read)(p_owner, 0UL, 0, p_data + ptr, len)) goto fail;
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

BOOL pifSpiDevice_ReadRegByte(PifDevice* p_owner, uint8_t reg, uint8_t* p_data)
{
	return pifSpiDevice_Read(p_owner, reg, 1, p_data, 1);
}

BOOL pifSpiDevice_ReadRegWord(PifDevice* p_owner, uint8_t reg, uint16_t* p_data)
{
	uint8_t tmp[2];

	if (!pifSpiDevice_Read(p_owner, reg, 1, tmp, 2)) return FALSE;
	*p_data = (tmp[0] << 8) + tmp[1];
	return TRUE;
}

BOOL pifSpiDevice_ReadRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size)
{
	return pifSpiDevice_Read(p_owner, reg, 1, p_data, size);
}

BOOL pifSpiDevice_ReadRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t* p_data)
{
	uint8_t tmp;

	if (!pifSpiDevice_Read(p_owner, reg, 1, &tmp, 1)) return FALSE;
	*p_data = tmp & mask;
	return TRUE;
}

BOOL pifSpiDevice_ReadRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t* p_data)
{
	uint8_t tmp[2];

	if (!pifSpiDevice_Read(p_owner, reg, 1, tmp, 2)) return FALSE;
	*p_data = ((tmp[0] << 8) + tmp[1]) & mask;
	return TRUE;
}

BOOL pifSpiDevice_Write(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifSpiDevice* p_device = (PifSpiDevice*)p_owner;
	PifSpiPort* p_port = p_device->_p_port;
	uint8_t len;
	size_t ptr, remain;

	if (!p_port->act_write) return FALSE;

	ptr = 0;
	remain = size;
	while (remain) {
		len = remain > p_port->__max_transfer_size ? p_port->__max_transfer_size : remain;
		if (!ptr) {
			if (!(*p_port->act_write)(p_owner, iaddr, isize, p_data, len)) goto fail;
		}
		else {
			if (!(*p_port->act_write)(p_owner, 0UL, 0, p_data + ptr, len)) goto fail;
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

BOOL pifSpiDevice_WriteRegByte(PifDevice* p_owner, uint8_t reg, uint8_t data)
{
	return pifSpiDevice_Write(p_owner, reg, 1, &data, 1);
}

BOOL pifSpiDevice_WriteRegWord(PifDevice* p_owner, uint8_t reg, uint16_t data)
{
	uint8_t tmp[2];

	tmp[0] = data >> 8;
	tmp[1] = data & 0xFF;
	if (!pifSpiDevice_Write(p_owner, reg, 1, tmp, 2)) return FALSE;
    return TRUE;
}

BOOL pifSpiDevice_WriteRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size)
{
	return pifSpiDevice_Write(p_owner, reg, 1, p_data, size);
}

BOOL pifSpiDevice_WriteRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t data)
{
	uint8_t tmp, org;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifSpiDevice_Read(p_owner, reg, 1, &org, 1)) return FALSE;

	if ((org & mask) != data) {
		tmp = (org & ~mask) | data;
		if (!pifSpiDevice_Write(p_owner, reg, 1, &tmp, 1)) return FALSE;
	}
    return TRUE;
}

BOOL pifSpiDevice_WriteRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t data)
{
	uint8_t tmp[2];
	uint16_t org;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifSpiDevice_ReadRegWord(p_owner, reg, &org)) return FALSE;

	if ((org & mask) != data) {
		org = (org & ~mask) | data;
		tmp[0] = org >> 8;
		tmp[1] = org & 0xFF;
		if (!pifSpiDevice_Write(p_owner, reg, 1, tmp, 2)) return FALSE;
	}
    return TRUE;
}
