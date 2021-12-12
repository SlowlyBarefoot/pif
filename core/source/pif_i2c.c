#include "pif_i2c.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif


static void _evtClear(char* p_data)
{
	PifI2cDevice* p_device = (PifI2cDevice*)p_data;

	if (p_device->p_data) {
		free(p_device->p_data);
		p_device->p_data = NULL;
	}
}

BOOL pifI2cPort_Init(PifI2cPort* p_owner, PifId id, uint8_t size)
{
	if (!p_owner || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifI2cPort));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    if (!pifFixList_Init(&p_owner->__devices, sizeof(PifI2cDevice), size)) goto fail;
    return TRUE;

fail:
	pifI2cPort_Clear(p_owner);
	return FALSE;
}

void pifI2cPort_Clear(PifI2cPort* p_owner)
{
	pifFixList_Clear(&p_owner->__devices, _evtClear);
}

PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, uint16_t data_size)
{
	if (!p_owner || !data_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	PifI2cDevice* p_device = (PifI2cDevice*)pifFixList_AddFirst(&p_owner->__devices);
    if (!p_device) return FALSE;

    p_device->p_data = calloc(sizeof(uint8_t), data_size);
    if (!p_device->p_data) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    p_device->data_size = data_size;
    p_device->__p_port = p_owner;
    return p_device;

fail:
	pifI2cPort_RemoveDevice(p_owner, p_device);
	return NULL;
}

void pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device)
{
	if (p_device) {
		if (p_device->p_data) {
			free(p_device->p_data);
			p_device->p_data = NULL;
		}
		pifFixList_Remove(&p_owner->__devices, p_device);
	}
}

#ifndef __PIF_NO_LOG__

void pifI2cPort_ScanAddress(PifI2cPort* p_owner)
{
	int i;
	PifI2cDevice device;

	device.__p_port = p_owner;
	for (i = 0; i < 127; i++) {
		device.addr = i;
		if (pifI2cDevice_Write(&device, 0)) {
			pifLog_Printf(LT_INFO, "I2C:%u Addr:%xh OK", __LINE__, i);
		}
	}
}

#endif

BOOL pifI2cDevice_Read(PifI2cDevice* p_owner, uint8_t size)
{
	PifI2cPort* p_port = p_owner->__p_port;

	if (!p_port->act_read) goto fail;
	if (p_port->__use_device) goto fail;

	p_port->__use_device = p_owner;
	p_owner->_state = IS_RUN;
	switch ((*p_port->act_read)(p_owner, size)) {
	case IR_WAIT:
		while (p_owner->_state == IS_RUN) {
			pifTaskManager_Yield();
		}
		if (p_owner->_state == IS_ERROR) goto fail;
		break;

	case IR_COMPLETE:
		break;

	case IR_ERROR:
		p_port->__use_device = NULL;
		goto fail;
	}
	p_port->__use_device = NULL;
	p_owner->_state = IS_IDLE;
	return TRUE;

fail:
	p_port->__use_device = NULL;
	p_owner->_state = IS_IDLE;
	pif_error = E_TRANSFER_FAILED;
	return FALSE;
}

BOOL pifI2cDevice_Write(PifI2cDevice* p_owner, uint8_t size)
{
	PifI2cPort* p_port = p_owner->__p_port;

	if (!p_port->act_write) goto fail;
	if (p_port->__use_device) goto fail;

	p_port->__use_device = p_owner;
	p_owner->_state = IS_RUN;
	switch ((*p_port->act_write)(p_owner, size)) {
	case IR_WAIT:
		while (p_owner->_state == IS_RUN) {
			pifTaskManager_Yield();
		}
		if (p_owner->_state == IS_ERROR) goto fail;
		break;

	case IR_COMPLETE:
		break;

	case IR_ERROR:
		p_port->__use_device = NULL;
		goto fail;
	}
	p_port->__use_device = NULL;
	p_owner->_state = IS_IDLE;
	return TRUE;

fail:
	p_port->__use_device = NULL;
	p_owner->_state = IS_IDLE;
	pif_error = E_TRANSFER_FAILED;
	return FALSE;
}

BOOL pifI2cDevice_ReadRegByte(PifI2cDevice* p_owner, uint8_t reg, uint8_t* p_data)
{
	p_owner->p_data[0] = reg;
	if (!pifI2cDevice_Write(p_owner, 1)) return FALSE;
	if (!pifI2cDevice_Read(p_owner, 1)) return FALSE;
	*p_data = p_owner->p_data[0];
	return TRUE;
}

BOOL pifI2cDevice_ReadRegWord(PifI2cDevice* p_owner, uint8_t reg, uint16_t* p_data)
{
	p_owner->p_data[0] = reg;
	if (!pifI2cDevice_Write(p_owner, 1)) return FALSE;
	if (!pifI2cDevice_Read(p_owner, 2)) return FALSE;
	*p_data = (p_owner->p_data[0] << 8) + p_owner->p_data[1];
	return TRUE;
}

BOOL pifI2cDevice_ReadRegBytes(PifI2cDevice* p_owner, uint8_t reg, uint8_t* p_data, uint8_t size)
{
	if (size > p_owner->data_size) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}
	p_owner->p_data[0] = reg;
	if (!pifI2cDevice_Write(p_owner, 1)) return FALSE;
	if (!pifI2cDevice_Read(p_owner, size)) return FALSE;
	memcpy(p_data, p_owner->p_data, size);
	return TRUE;
}

BOOL pifI2cDevice_ReadRegBit8(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint8_t* p_data)
{
	uint8_t shift, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	p_owner->p_data[0] = reg;
	if (!pifI2cDevice_Write(p_owner, 1)) return FALSE;
	if (!pifI2cDevice_Read(p_owner, 1)) return FALSE;
	*p_data = (p_owner->p_data[0] >> shift) & mask;
	return TRUE;
}

BOOL pifI2cDevice_ReadRegBit16(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint16_t* p_data)
{
	uint8_t shift;
	uint16_t mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	p_owner->p_data[0] = reg;
	if (!pifI2cDevice_Write(p_owner, 1)) return FALSE;
	if (!pifI2cDevice_Read(p_owner, 2)) return FALSE;
	*p_data = (((p_owner->p_data[0] << 8) + p_owner->p_data[1]) >> shift) & mask;
	return TRUE;
}

BOOL pifI2cDevice_WriteRegByte(PifI2cDevice* p_owner, uint8_t reg, uint8_t data)
{
	p_owner->p_data[0] = reg;
	p_owner->p_data[1] = data;
	if (!pifI2cDevice_Write(p_owner, 2)) return FALSE;
    return TRUE;
}

BOOL pifI2cDevice_WriteRegWord(PifI2cDevice* p_owner, uint8_t reg, uint16_t data)
{
	p_owner->p_data[0] = reg;
	p_owner->p_data[1] = data >> 8;
	p_owner->p_data[2] = data & 0xFF;
	if (!pifI2cDevice_Write(p_owner, 3)) return FALSE;
    return TRUE;
}

BOOL pifI2cDevice_WriteRegBytes(PifI2cDevice* p_owner, uint8_t reg, uint8_t* p_data, uint8_t size)
{
	if (size + 1 > p_owner->data_size) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}
	p_owner->p_data[0] = reg;
	memcpy(&p_owner->p_data[1], p_data, size);
	if (!pifI2cDevice_Write(p_owner, size + 1)) return FALSE;
    return TRUE;
}

BOOL pifI2cDevice_WriteRegBit8(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint8_t data)
{
	uint8_t org, shift, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifI2cDevice_ReadRegByte(p_owner, reg, &org)) return FALSE;

	if (((org >> shift) & mask) != data) {
		p_owner->p_data[0] = reg;
		p_owner->p_data[1] = (org & ~(mask << shift)) | (data << shift);
		if (!pifI2cDevice_Write(p_owner, 2)) return FALSE;
	}
    return TRUE;
}

BOOL pifI2cDevice_WriteRegBit16(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint16_t data)
{
	uint8_t shift;
	uint16_t org, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifI2cDevice_ReadRegWord(p_owner, reg, &org)) return FALSE;

	if (((org >> shift) & mask) != data) {
		p_owner->p_data[0] = reg;
		org = (org & ~(mask << shift)) | (data << shift);
		p_owner->p_data[1] = org >> 8;
		p_owner->p_data[2] = org & 0xFF;
		if (!pifI2cDevice_Write(p_owner, 3)) return FALSE;
	}
    return TRUE;
}

void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result)
{
	if (!p_owner->__use_device) return;
	p_owner->__use_device->_state = result ? IS_COMPLETE : IS_ERROR;
	p_owner->__use_device = NULL;
}
