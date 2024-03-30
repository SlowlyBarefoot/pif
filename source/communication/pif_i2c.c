#include "communication/pif_i2c.h"
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_task.h"


BOOL pifI2cPort_Init(PifI2cPort *p_owner, PifId id, uint8_t device_count, uint16_t max_transfer_size, void *p_client)
{
	if (!p_owner || !device_count || !max_transfer_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifI2cPort));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__max_transfer_size = max_transfer_size;
    if (!pifObjArray_Init(&p_owner->__devices, sizeof(PifI2cDevice), device_count, NULL)) goto fail;
	p_owner->_p_client = p_client;
    return TRUE;

fail:
	pifI2cPort_Clear(p_owner);
	return FALSE;
}

void pifI2cPort_Clear(PifI2cPort* p_owner)
{
	pifObjArray_Clear(&p_owner->__devices);
}

PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, uint8_t addr)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	PifObjArrayIterator it = pifObjArray_Add(&p_owner->__devices);
    if (!it) return FALSE;

    PifI2cDevice* p_device = (PifI2cDevice*)it->data;
    p_device->_p_port = p_owner;
	p_device->addr = addr;
    p_device->timeout = 10;		// 10ms
    return p_device;
}

void pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device)
{
	if (p_device) {
		pifObjArray_Remove(&p_owner->__devices, p_device);
		p_device = NULL;
	}
}

PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr)
{
	static PifI2cDevice device;

	device._p_port = p_owner;
	device.addr = addr;
	device._state = IS_IDLE;
	return &device;
}

#ifndef PIF_NO_LOG

void pifI2cPort_ScanAddress(PifI2cPort* p_owner)
{
	uint8_t data;
	int i, count = 0;
	PifI2cDevice device;

	device._p_port = p_owner;
	for (i = 1; i < 127; i++) {
		device.addr = i;
		device._state = IS_IDLE;
		data = 0xFF;
		if (pifI2cDevice_Write(&device, 0, 0, &data, 1)) {
			pifLog_Printf(LT_INFO, "I2C Addr:%Xh %u", i, data);
			count++;
		}
		pif_Delay1ms(10);
	}
	if (count) {
		pifLog_Printf(LT_INFO, "I2C %d found", count);
	}
	else {
		pifLog_Print(LT_INFO, "I2C Not found");
	}
}

#endif

BOOL pifI2cDevice_Read(PifI2cDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifI2cPort* p_port = p_owner->_p_port;
	uint8_t len;
	uint32_t timer1ms;
	size_t ptr;
#ifndef PIF_NO_LOG
	int line;
#endif

	if (!p_port->act_read) return FALSE;
	if (p_port->__use_device) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "I2CR:%u Addr:%Xh Use Addr:%Xh", __LINE__, p_owner->addr, p_port->__use_device->addr);
#endif
		return FALSE;
	}

	p_port->__use_device = p_owner;
	p_owner->_state = IS_RUN;
	ptr = 0;
	while (size) {
		len = size > p_port->__max_transfer_size ? p_port->__max_transfer_size : size;
		switch ((*p_port->act_read)(p_owner, iaddr + ptr, isize, p_data + ptr, len)) {
		case IR_WAIT:
			timer1ms = pif_cumulative_timer1ms;
			while (p_owner->_state == IS_RUN) {
				if (pif_cumulative_timer1ms - timer1ms > p_owner->timeout) {
#ifndef PIF_NO_LOG
					line = __LINE__;
#endif
					goto fail;
				}
			}
			if (p_owner->_state == IS_ERROR) {
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case IR_COMPLETE:
			break;

		case IR_ERROR:
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
		ptr += len;
		size -= len;
	}
	p_port->__use_device = NULL;
	p_owner->_state = IS_IDLE;
	return TRUE;

fail:
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "I2CR:%u A:%Xh R:%Xh E:%u", line, p_owner->addr, iaddr, pif_error);
#endif
	p_port->__use_device = NULL;
	p_port->error_count++;
	p_owner->_state = IS_IDLE;
	pif_error = E_TRANSFER_FAILED;
	return FALSE;
}

BOOL pifI2cDevice_ReadRegByte(PifI2cDevice* p_owner, uint8_t reg, uint8_t* p_data)
{
	return pifI2cDevice_Read(p_owner, reg, 1, p_data, 1);
}

BOOL pifI2cDevice_ReadRegWord(PifI2cDevice* p_owner, uint8_t reg, uint16_t* p_data)
{
	uint8_t tmp[2];

	if (!pifI2cDevice_Read(p_owner, reg, 1, tmp, 2)) return FALSE;
	*p_data = (tmp[0] << 8) + tmp[1];
	return TRUE;
}

BOOL pifI2cDevice_ReadRegBytes(PifI2cDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size)
{
	return pifI2cDevice_Read(p_owner, reg, 1, p_data, size);
}

BOOL pifI2cDevice_ReadRegBit8(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint8_t* p_data)
{
	uint8_t tmp, shift, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (!pifI2cDevice_Read(p_owner, reg, 1, &tmp, 1)) return FALSE;
	*p_data = (tmp >> shift) & mask;
	return TRUE;
}

BOOL pifI2cDevice_ReadRegBit16(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint16_t* p_data)
{
	uint8_t tmp[2], shift;
	uint16_t mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (!pifI2cDevice_Read(p_owner, reg, 1, tmp, 2)) return FALSE;
	*p_data = (((tmp[0] << 8) + tmp[1]) >> shift) & mask;
	return TRUE;
}

BOOL pifI2cDevice_Write(PifI2cDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifI2cPort* p_port = p_owner->_p_port;
	uint8_t len;
	uint32_t timer1ms;
	size_t ptr;
#ifndef PIF_NO_LOG
	int line;
#endif

	if (!p_port->act_write) return FALSE;
	if (p_port->__use_device) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "I2CW:%u Addr:%Xh Use Addr:%Xh", __LINE__, p_owner->addr, p_port->__use_device->addr);
#endif
		return FALSE;
	}

	p_port->__use_device = p_owner;
	p_owner->_state = IS_RUN;
	ptr = 0;
	while (size) {
		len = size > p_port->__max_transfer_size ? p_port->__max_transfer_size : size;
		switch ((*p_port->act_write)(p_owner, iaddr + ptr, isize, p_data + ptr, len)) {
		case IR_WAIT:
			timer1ms = pif_cumulative_timer1ms;
			while (p_owner->_state == IS_RUN) {
				if (pif_cumulative_timer1ms - timer1ms > p_owner->timeout) {
#ifndef PIF_NO_LOG
					line = __LINE__;
#endif
					goto fail;
				}
			}
			if (p_owner->_state == IS_ERROR) {
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case IR_COMPLETE:
			break;

		case IR_ERROR:
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
		ptr += len;
		size -= len;
	}
	p_port->__use_device = NULL;
	p_owner->_state = IS_IDLE;
	return TRUE;

fail:
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "I2CW:%u A:%Xh R:%Xh E:%u", line, p_owner->addr, iaddr, pif_error);
#endif
	p_port->__use_device = NULL;
	p_port->error_count++;
	p_owner->_state = IS_IDLE;
	pif_error = E_TRANSFER_FAILED;
	return FALSE;
}

BOOL pifI2cDevice_WriteRegByte(PifI2cDevice* p_owner, uint8_t reg, uint8_t data)
{
	return pifI2cDevice_Write(p_owner, reg, 1, &data, 1);
}

BOOL pifI2cDevice_WriteRegWord(PifI2cDevice* p_owner, uint8_t reg, uint16_t data)
{
	uint8_t tmp[2];

	tmp[0] = data >> 8;
	tmp[1] = data & 0xFF;
	if (!pifI2cDevice_Write(p_owner, reg, 1, tmp, 2)) return FALSE;
    return TRUE;
}

BOOL pifI2cDevice_WriteRegBytes(PifI2cDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size)
{
	return pifI2cDevice_Write(p_owner, reg, 1, p_data, size);
}

BOOL pifI2cDevice_WriteRegBit8(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint8_t data)
{
	uint8_t tmp, org, shift, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifI2cDevice_Read(p_owner, reg, 1, &org, 1)) return FALSE;

	if (((org >> shift) & mask) != data) {
		tmp = (org & ~(mask << shift)) | (data << shift);
		if (!pifI2cDevice_Write(p_owner, reg, 1, &tmp, 1)) return FALSE;
	}
    return TRUE;
}

BOOL pifI2cDevice_WriteRegBit16(PifI2cDevice* p_owner, uint8_t reg, PifI2cRegField field, uint16_t data)
{
	uint8_t tmp[2], shift;
	uint16_t org, mask;

	shift = field >> 8;
	mask = (1 << (field & 0xFF)) - 1;

	if (data > mask) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	if (!pifI2cDevice_ReadRegWord(p_owner, reg, &org)) return FALSE;

	if (((org >> shift) & mask) != data) {
		org = (org & ~(mask << shift)) | (data << shift);
		tmp[0] = org >> 8;
		tmp[1] = org & 0xFF;
		if (!pifI2cDevice_Write(p_owner, reg, 1, tmp, 2)) return FALSE;
	}
    return TRUE;
}

void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result)
{
	if (!p_owner->__use_device) return;
	p_owner->__use_device->_state = result ? IS_COMPLETE : IS_ERROR;
}
