#include "pifGpio.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stGpio *s_pstGpio = NULL;
static uint8_t s_ucGpioSize;
static uint8_t s_ucGpioPos;


/**
 * @fn pifGpio_Init
 * @brief
 * @param pstTimer
 * @param ucSize
 * @return
 */
BOOL pifGpio_Init(uint8_t ucSize)
{
    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstGpio = calloc(sizeof(PIF_stGpio), ucSize);
    if (!s_pstGpio) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucGpioSize = ucSize;
    s_ucGpioPos = 0;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GPIO:%u S:%u EC:%d", __LINE__, ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifGpio_Exit
 * @brief
 */
void pifGpio_Exit()
{
    if (s_pstGpio) {
    	free(s_pstGpio);
        s_pstGpio = NULL;
    }
}

/**
 * @fn pifGpio_AddIn
 * @brief
 * @param usPifId
 * @param ucCount
 * @param actIn
 * @return
 */
PIF_stGpio *pifGpio_AddIn(PIF_usId usPifId, uint8_t ucCount, PIF_actGpioIn actIn)
{
    if (s_ucGpioPos >= s_ucGpioSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucCount || ucCount > 8 || !actIn) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stGpio *pstOwner = &s_pstGpio[s_ucGpioPos];

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucGpioCount = ucCount;
    pstOwner->__actIn = actIn;

    s_ucGpioPos = s_ucGpioPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GPIO:%u(%u) C=%u EC:%d", __LINE__, usPifId, ucCount, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifGpio_AddOut
 * @brief
 * @param usPifId
 * @param ucCount
 * @param actOut
 * @return
 */
PIF_stGpio *pifGpio_AddOut(PIF_usId usPifId, uint8_t ucCount, PIF_actGpioOut actOut)
{
    if (s_ucGpioPos >= s_ucGpioSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucCount || ucCount > 8 || !actOut) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stGpio *pstOwner = &s_pstGpio[s_ucGpioPos];

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucGpioCount = ucCount;
    pstOwner->__actOut = actOut;

    s_ucGpioPos = s_ucGpioPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GPIO:%u(%u) C=%u EC:%d", __LINE__, usPifId, ucCount, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifGpio_ReadAll
 * @brief
 * @param pstOwner
 * @return
 */
uint8_t pifGpio_ReadAll(PIF_stGpio *pstOwner)
{
	return pstOwner->__actIn(pstOwner->_usPifId);
}

/**
 * @fn pifGpio_ReadBit
 * @brief
 * @param pstOwner
 * @param ucIndex
 * @return
 */
SWITCH pifGpio_ReadBit(PIF_stGpio *pstOwner, uint8_t ucIndex)
{
	return (pstOwner->__actIn(pstOwner->_usPifId) >> ucIndex) & 1;
}

/**
 * @fn pifGpio_WriteAll
 * @brief
 * @param pstOwner
 * @param ucState
 */
void pifGpio_WriteAll(PIF_stGpio *pstOwner, uint8_t ucState)
{
	pstOwner->__ucState = ucState;
	pstOwner->__actOut(pstOwner->_usPifId, ucState);
}

/**
 * @fn pifGpio_WriteBit
 * @brief
 * @param pstOwner
 * @param ucIndex
 * @param swState
 */
void pifGpio_WriteBit(PIF_stGpio *pstOwner, uint8_t ucIndex, SWITCH swState)
{
	if (swState) {
		pstOwner->__ucState |= 1 << ucIndex;
	}
	else {
		pstOwner->__ucState &= ~(1 << ucIndex);
	}
	pstOwner->__actOut(pstOwner->_usPifId, pstOwner->__ucState);
}
