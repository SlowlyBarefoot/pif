#include "pif_i2c.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif


BOOL pifI2c_Init(PifI2c* p_owner, PifId id, uint16_t data_size)
{
	if (!p_owner || !data_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifI2c));

	p_owner->p_data = calloc(sizeof(uint8_t), data_size);
    if (!p_owner->p_data) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->data_size = data_size;
    return TRUE;

fail:
	pifI2c_Clear(p_owner);
	return FALSE;
}

void pifI2c_Clear(PifI2c* p_owner)
{
	if (p_owner->p_data) {
		free(p_owner->p_data);
		p_owner->p_data = NULL;
	}
}

#ifndef __PIF_NO_LOG__

void pifI2c_ScanAddress(PifI2c* p_owner)
{
	int i;

	for (i = 0; i < 127; i++) {
		p_owner->addr = i;
		if (pifI2c_Write(p_owner, 0)) {
			pifLog_Printf(LT_INFO, "I2C:%u Addr:%xh OK", __LINE__, i);
		}
	}
}

#endif

BOOL pifI2c_Read(PifI2c* p_owner, uint8_t size)
{
	if (!p_owner->act_read) return FALSE;

	p_owner->_state_read = IS_RUN;
	if (!(*p_owner->act_read)(p_owner, size)) {
		p_owner->_state_read = IS_ERROR;
		return FALSE;
	}

	while (p_owner->_state_read == IS_RUN) {
		pifTaskManager_Yield();
	}
	return p_owner->_state_read == IS_COMPLETE;
}

BOOL pifI2c_Write(PifI2c* p_owner, uint8_t size)
{
	if (!p_owner->act_write) return FALSE;

	p_owner->_state_write = IS_RUN;
	if (!(*p_owner->act_write)(p_owner, size)) {
		p_owner->_state_write = IS_ERROR;
		return FALSE;
	}

	while (p_owner->_state_write == IS_RUN) {
		pifTaskManager_Yield();
	}
	return p_owner->_state_write == IS_COMPLETE;
}

void pifI2c_sigEndRead(PifI2c* p_owner, BOOL result)
{
	p_owner->_state_read = result ? IS_COMPLETE : IS_ERROR;
}

void pifI2c_sigEndWrite(PifI2c* p_owner, BOOL result)
{
	p_owner->_state_write = result ? IS_COMPLETE : IS_ERROR;
}
