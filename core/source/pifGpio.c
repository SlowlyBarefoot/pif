#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#include "pifGpio.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stGpio *s_pstGpio = NULL;
static uint8_t s_ucGpioSize;
static uint8_t s_ucGpioPos;


#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char *prefix[GpCsF_enCount] = { "GP" };

	for (int i = 0; i < s_ucGpioPos; i++) {
		PIF_stGpio *pstOwner = &s_pstGpio[i];
		if (pstOwner->__ucCsFlag) {
			for (int f = 0; f < GpCsF_enCount; f++) {
				pstOwner->__cCsIndex[f] = pifCollectSignal_AddDevice(pstOwner->_usPifId, CSVT_enWire, 1,
						prefix[f], pstOwner->__ucState);
			}
		}
	}
}

#endif

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

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enGpio, _AddDeviceInCollectSignal);
#endif
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

    pstOwner->__ucIndex = s_ucGpioPos;
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

    pstOwner->__ucIndex = s_ucGpioPos;
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

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifGpio_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifGpio_SetCsFlagAll(PIF_enGpioCsFlag enFlag)
{
    for (int i = 0; i < s_ucGpioPos; i++) {
    	s_pstGpio[i].__ucCsFlag |= enFlag;
    }
}

/**
 * @fn pifGpio_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifGpio_ResetCsFlagAll(PIF_enGpioCsFlag enFlag)
{
    for (int i = 0; i < s_ucGpioPos; i++) {
    	s_pstGpio[i].__ucCsFlag &= ~enFlag;
    }
}

/**
 * @fn pifGpio_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifGpio_SetCsFlagEach(PIF_stGpio *pstOwner, PIF_enGpioCsFlag enFlag)
{
	pstOwner->__ucCsFlag |= enFlag;
}

/**
 * @fn pifGpio_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifGpio_ResetCsFlagEach(PIF_stGpio *pstOwner, PIF_enGpioCsFlag enFlag)
{
	pstOwner->__ucCsFlag &= ~enFlag;
}

#endif
