#include "pifI2c.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static void _Clean(PifI2c* p_owner)
{
	if (p_owner->p_data) {
		free(p_owner->p_data);
		p_owner->p_data = NULL;
	}
}

/**
 * @fn pifI2c_Create
 * @brief
 * @param id
 * @param data_size
 * @return
 */
PifI2c* pifI2c_Create(PifId id, uint16_t data_size)
{
	PifI2c *p_owner = calloc(sizeof(PifI2c), 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	if (!pifI2c_Init(p_owner, id, data_size)) goto fail;
    return p_owner;

fail:
	if (p_owner) free(p_owner);
	return NULL;
}

/**
 * @fn pifI2c_Destroy
 * @brief
 * @param pp_owner
 */
void pifI2c_Destroy(PifI2c** pp_owner)
{
	if (*pp_owner) {
		_Clean(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifI2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param data_size
 * @return
 */
BOOL pifI2c_Init(PifI2c* p_owner, PifId id, uint16_t data_size)
{
	_Clean(p_owner);

	if (!data_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->p_data = calloc(sizeof(uint8_t), data_size);
    if (!p_owner->p_data) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->data_size = data_size;
    p_owner->_state_read = IS_IDLE;
    p_owner->_state_write = IS_IDLE;
    return TRUE;
}

#ifndef __PIF_NO_LOG__

/**
 * @fn pifI2c_ScanAddress
 * @brief
 * @param p_owner
 */
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

/**
 * @fn pifI2c_Read
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifI2c_Read(PifI2c* p_owner, uint8_t size)
{
	if (!p_owner->__act_read) return FALSE;

	p_owner->_state_read = IS_RUN;
	if (!(*p_owner->__act_read)(p_owner, size)) {
		p_owner->_state_read = IS_ERROR;
		return FALSE;
	}

	while (p_owner->_state_read == IS_RUN) {
		pifTaskManager_Yield();
	}
	return p_owner->_state_read == IS_COMPLETE;
}

/**
 * @fn pifI2c_Write
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifI2c_Write(PifI2c* p_owner, uint8_t size)
{
	if (!p_owner->__act_write) return FALSE;

	p_owner->_state_write = IS_RUN;
	if (!(*p_owner->__act_write)(p_owner, size)) {
		p_owner->_state_write = IS_ERROR;
		return FALSE;
	}

	while (p_owner->_state_write == IS_RUN) {
		pifTaskManager_Yield();
	}
	return p_owner->_state_write == IS_COMPLETE;
}

/**
 * @fn pifI2c_sigEndRead
 * @brief
 * @param p_owner
 * @param result
 */
void pifI2c_sigEndRead(PifI2c* p_owner, BOOL result)
{
	p_owner->_state_read = result ? IS_COMPLETE : IS_ERROR;
}

/**
 * @fn pifI2c_sigEndWrite
 * @brief
 * @param p_owner
 * @param result
 */
void pifI2c_sigEndWrite(PifI2c* p_owner, BOOL result)
{
	p_owner->_state_write = result ? IS_COMPLETE : IS_ERROR;
}

/**
 * @fn pifI2c_AttachAction
 * @brief
 * @param p_owner
 * @param act_read
 * @param act_write
 */
void pifI2c_AttachAction(PifI2c* p_owner, PifActI2cRead act_read, PifActI2cWrite act_write)
{
	p_owner->__act_read = act_read;
	p_owner->__act_write = act_write;
}
