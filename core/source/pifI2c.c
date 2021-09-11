#include "pifI2c.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


/**
 * @fn pifI2c_Add
 * @brief
 * @param pstOwner
 * @param usPifId
 * @param ucDataSize
 * @return
 */
BOOL pifI2c_Add(PIF_stI2c *pstOwner, PIF_usId usPifId, uint16_t ucDataSize)
{
	pstOwner->pucData = calloc(sizeof(uint8_t), ucDataSize);
    if (!pstOwner->pucData) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucDataSize = ucDataSize;
    pstOwner->_enStateRead = IS_enIdle;
    pstOwner->_enStateWrite = IS_enIdle;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "I2C:%u S:%u EC:%d", __LINE__, ucDataSize, pif_enError);
#endif
	return FALSE;
}

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
		pifTask_Yield();
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
		pifTask_Yield();
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
