#include "pifI2c.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static void _Clean(PIF_stI2c *pstOwner)
{
	if (pstOwner->pucData) {
		free(pstOwner->pucData);
		pstOwner->pucData = NULL;
	}
}

/**
 * @fn pifI2c_Create
 * @brief
 * @param usPifId
 * @param ucDataSize
 * @return
 */
PIF_stI2c *pifI2c_Create(PIF_usId usPifId, uint16_t ucDataSize)
{
	PIF_stI2c *pstOwner = calloc(sizeof(PIF_stI2c), 1);
	if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	if (!pifI2c_Init(pstOwner, usPifId, ucDataSize)) goto fail;
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
	return NULL;
}

/**
 * @fn pifI2c_Destroy
 * @brief
 * @param pstOwner
 */
void pifI2c_Destroy(PIF_stI2c **ppstOwner)
{
	if (*ppstOwner) {
		_Clean(*ppstOwner);
		free(*ppstOwner);
		*ppstOwner = NULL;
	}
}

/**
 * @fn pifI2c_Init
 * @brief
 * @param pstOwner
 * @param usPifId
 * @param ucDataSize
 * @return
 */
BOOL pifI2c_Init(PIF_stI2c *pstOwner, PIF_usId usPifId, uint16_t ucDataSize)
{
	_Clean(pstOwner);

	if (!ucDataSize) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	pstOwner->pucData = calloc(sizeof(uint8_t), ucDataSize);
    if (!pstOwner->pucData) {
		pif_enError = E_enOutOfHeap;
		return FALSE;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucDataSize = ucDataSize;
    pstOwner->_enStateRead = IS_enIdle;
    pstOwner->_enStateWrite = IS_enIdle;
    return TRUE;
}

#ifndef __PIF_NO_LOG__

/**
 * @fn pifI2c_ScanAddress
 * @brief
 * @param pstOwner
 * @return
 */
void pifI2c_ScanAddress(PIF_stI2c *pstOwner)
{
	int i;

	for (i = 0; i < 127; i++) {
		pstOwner->ucAddr = i;
		if (pifI2c_Write(pstOwner, 0)) {
			pifLog_Printf(LT_enInfo, "I2C:%u Addr:%xh OK", __LINE__, i);
		}
	}
}

#endif

/**
 * @fn pifI2c_Read
 * @brief
 * @param pstOwner
 * @param ucSize
 * @return
 */
BOOL pifI2c_Read(PIF_stI2c *pstOwner, uint8_t ucSize)
{
	if (!pstOwner->__actRead) return FALSE;

	pstOwner->_enStateRead = IS_enRun;
	if (!(*pstOwner->__actRead)(pstOwner, ucSize)) {
		pstOwner->_enStateRead = IS_enError;
		return FALSE;
	}

	while (pstOwner->_enStateRead == IS_enRun) {
		pifTaskManager_Yield();
	}
	return pstOwner->_enStateRead == IS_enComplete;
}

/**
 * @fn pifI2c_Write
 * @brief
 * @param pstOwner
 * @param ucSize
 * @return
 */
BOOL pifI2c_Write(PIF_stI2c *pstOwner, uint8_t ucSize)
{
	if (!pstOwner->__actWrite) return FALSE;

	pstOwner->_enStateWrite = IS_enRun;
	if (!(*pstOwner->__actWrite)(pstOwner, ucSize)) {
		pstOwner->_enStateWrite = IS_enError;
		return FALSE;
	}

	while (pstOwner->_enStateWrite == IS_enRun) {
		pifTaskManager_Yield();
	}
	return pstOwner->_enStateWrite == IS_enComplete;
}

/**
 * @fn pifI2c_sigEndRead
 * @brief
 * @param pstOwner
 * @param bResult
 */
void pifI2c_sigEndRead(PIF_stI2c *pstOwner, BOOL bResult)
{
	pstOwner->_enStateRead = bResult ? IS_enComplete : IS_enError;
}

/**
 * @fn pifI2c_sigEndWrite
 * @brief
 * @param pstOwner
 * @param bResult
 */
void pifI2c_sigEndWrite(PIF_stI2c *pstOwner, BOOL bResult)
{
	pstOwner->_enStateWrite = bResult ? IS_enComplete : IS_enError;
}

/**
 * @fn pifI2c_AttachAction
 * @brief
 * @param pstOwner
 * @param actRead
 * @param actWrite
 */
void pifI2c_AttachAction(PIF_stI2c *pstOwner, PIF_actI2cRead actRead, PIF_actI2cWrite actWrite)
{
	pstOwner->__actRead = actRead;
	pstOwner->__actWrite = actWrite;
}
