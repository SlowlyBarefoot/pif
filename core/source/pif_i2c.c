#include "pif_i2c.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif


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
	pifFixList_Clear(&p_owner->__devices);
}

PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, PifId id, uint16_t data_size)
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

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_device->_id = id;
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

void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result)
{
	if (!p_owner->__use_device) return;
	p_owner->__use_device->_state = result ? IS_COMPLETE : IS_ERROR;
	p_owner->__use_device = NULL;
}
