#include "pifAds1x1x.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#define DEFAULT_I2C_ADDR	0x48


static BOOL _ReadWord(PIF_stAds1x1x *pstOwner, const PIF_enAds1x1xReg enReg, uint16_t *pusData)
{
	pstOwner->_stI2c.pucData[0] = enReg;
	if (!pifI2c_Write(&pstOwner->_stI2c, 1)) return FALSE;
	if (!pifI2c_Read(&pstOwner->_stI2c, 2)) return FALSE;
	*pusData = (pstOwner->_stI2c.pucData[0] << 8) + pstOwner->_stI2c.pucData[1];
	return TRUE;
}

static BOOL _WriteWord(PIF_stAds1x1x *pstOwner, const PIF_enAds1x1xReg enReg, uint16_t usData)
{
	pstOwner->_stI2c.pucData[0] = enReg;
	pstOwner->_stI2c.pucData[1] = usData >> 8;
	pstOwner->_stI2c.pucData[2] = usData & 0xFF;
	return pifI2c_Write(&pstOwner->_stI2c, 3);
}

static double _ConvertVoltage(PIF_stAds1x1x *pstOwner)
{
    switch (pstOwner->__stConfig.bt.PGA) {
        case ACP_enFSR_6_144V: return 6.144 / (0x7FFF >> pstOwner->__ucBitOffset);
        case ACP_enFSR_4_096V: return 4.096 / (0x7FFF >> pstOwner->__ucBitOffset);
        case ACP_enFSR_2_048V: return 2.048 / (0x7FFF >> pstOwner->__ucBitOffset);
        case ACP_enFSR_1_024V: return 1.024 / (0x7FFF >> pstOwner->__ucBitOffset);
        case ACP_enFSR_0_512V: return 0.512 / (0x7FFF >> pstOwner->__ucBitOffset);
        case ACP_enFSR_0_256V: return 0.256 / (0x7FFF >> pstOwner->__ucBitOffset);
    }
    return 2.048 / (0x7FFF >> pstOwner->__ucBitOffset);
}

static uint32_t _ConversionDelay(PIF_stAds1x1x *pstOwner)
{
	uint16_t usDataRate = 0;

    if (pstOwner->__unResolution == 12) {
        switch (pstOwner->__stConfig.bt.DR) {
            case ACD_enDR_12B_0128_SPS: usDataRate = 128; break;
            case ACD_enDR_12B_0250_SPS: usDataRate = 250; break;
            case ACD_enDR_12B_0490_SPS: usDataRate = 490; break;
            case ACD_enDR_12B_0920_SPS: usDataRate = 920; break;
            case ACD_enDR_12B_1600_SPS: usDataRate = 1600; break;
            case ACD_enDR_12B_2400_SPS: usDataRate = 2400; break;
            case ACD_enDR_12B_3300_SPS: usDataRate = 3300; break;
        }
    }
    else {
        switch (pstOwner->__stConfig.bt.DR) {
            case ACD_enDR_16B_0008_SPS: usDataRate = 8; break;
            case ACD_enDR_16B_0016_SPS: usDataRate = 16; break;
            case ACD_enDR_16B_0032_SPS: usDataRate = 32; break;
            case ACD_enDR_16B_0064_SPS: usDataRate = 64; break;
            case ACD_enDR_16B_0128_SPS: usDataRate = 128; break;
            case ACD_enDR_16B_0250_SPS: usDataRate = 250; break;
            case ACD_enDR_16B_0475_SPS: usDataRate = 475; break;
            case ACD_enDR_16B_0860_SPS: usDataRate = 860; break;
        }
    }
    if (usDataRate) {
		if (pif_actTimer1us) {
			return (1000000UL - 1) / usDataRate + 1;
		}
		else {
			return (1000UL - 1) / usDataRate + 1;
		}
    }
    return 0;
}

/**
 * @fn pifAds1x1x_Create
 * @brief
 * @param usPifId
 * @param enType
 * @return
 */
PIF_stAds1x1x *pifAds1x1x_Create(PIF_usId usPifId, PIF_enAds1x1xType enType)
{
    PIF_stAds1x1x *pstOwner = NULL;

    pstOwner = calloc(sizeof(PIF_stAds1x1x), 1);
    if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    if (!pifI2c_Init(&pstOwner->_stI2c, usPifId, 4)) goto fail;

    pstOwner->_stI2c.ucAddr = DEFAULT_I2C_ADDR;
    switch (enType) {
    case AT_en1115: pstOwner->__unResolution = 16; pstOwner->__unChannels = 4; break;
    case AT_en1114: pstOwner->__unResolution = 16; pstOwner->__unChannels = 1; break;
    case AT_en1113: pstOwner->__unResolution = 16; pstOwner->__unChannels = 1; break;
    case AT_en1015: pstOwner->__unResolution = 12; pstOwner->__unChannels = 4; break;
    case AT_en1014: pstOwner->__unResolution = 12; pstOwner->__unChannels = 1; break;
    case AT_en1013: pstOwner->__unResolution = 12; pstOwner->__unChannels = 1; break;
    default: goto fail;
    }
    pstOwner->_enType = enType;
    pstOwner->__ucBitOffset = pstOwner->__unResolution == 12 ? 4 : 0;
    _ReadWord(pstOwner, AR_enCONFIG, &pstOwner->__stConfig.usAll);
    pstOwner->dConvertVoltage = _ConvertVoltage(pstOwner);
    pstOwner->__unConversionDelay = _ConversionDelay(pstOwner);
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "ADS1x1x:%u(%u) EC:%d", __LINE__, usPifId, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifAds1x1x_Destroy
 * @brief
 * @param ppstOwner
 */
void pifAds1x1x_Destroy(PIF_stAds1x1x **ppstOwner)
{
    if (*ppstOwner) {
    	if ((*ppstOwner)->_stI2c.pucData) {
        	free((*ppstOwner)->_stI2c.pucData);
        	(*ppstOwner)->_stI2c.pucData = NULL;
    	}
    	free(*ppstOwner);
    	*ppstOwner = NULL;
    }
}

/**
 * @fn pifAds1x1x_SetAddress
 * @brief
 * @param pstOwner
 * @param ucAddr
 */
void pifAds1x1x_SetAddress(PIF_stAds1x1x *pstOwner, uint8_t ucAddr)
{
	pstOwner->_stI2c.ucAddr = ucAddr;
}

/**
 * @fn pifAds1x1x_Read
 * @brief
 * @param pstOwner
 * @return
 */
int16_t pifAds1x1x_Read(PIF_stAds1x1x *pstOwner)
{
	uint16_t usData;

	if (!_ReadWord(pstOwner, AR_enCONVERSION, &usData)) return 0;
	return usData >> pstOwner->__ucBitOffset;
}

/**
 * @fn pifAds1x1x_ReadMux
 * @brief
 * @param pstOwner
 * @param enMux
 * @return
 */
int16_t pifAds1x1x_ReadMux(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMux enMux)
{
	uint16_t usData;
	PIF_stAds1x1xConfig stConfig;

	if (pstOwner->__unChannels == 1 || pstOwner->__stConfig.bt.MODE == ACM_enCONTINUOUS) return 0;

	pstOwner->__stConfig.bt.MUX = enMux;
	stConfig.usAll = pstOwner->__stConfig.usAll;
	stConfig.bt.OS_SSCS = 1;
	if (!_WriteWord(pstOwner, AR_enCONFIG, stConfig.usAll)) return 0;
	if (pstOwner->__unConversionDelay) {
		if (pif_actTimer1us) {
			pifTaskManager_YieldUs(pstOwner->__unConversionDelay);
		}
		else {
			pifTaskManager_YieldMs(pstOwner->__unConversionDelay);
		}
	}
	if (!_ReadWord(pstOwner, AR_enCONVERSION, &usData)) return 0;
	return usData >> pstOwner->__ucBitOffset;
}

/**
 * @fn pifAds1x1x_Voltage
 * @brief
 * @param pstOwner
 * @return
 */
double pifAds1x1x_Voltage(PIF_stAds1x1x *pstOwner)
{
    return (double)pifAds1x1x_Read(pstOwner) * pstOwner->dConvertVoltage;
}

/**
 * @fn pifAds1x1x_VoltageMux
 * @brief
 * @param pstOwner
 * @param enMux
 * @return
 */
double pifAds1x1x_VoltageMux(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMux enMux)
{
    return (double)pifAds1x1x_ReadMux(pstOwner, enMux) * pstOwner->dConvertVoltage;
}

/**
 * @fn pifAds1x1x_SetConfig
 * @brief
 * @param pstOwner
 * @param pstConfig
 * @return
 */
BOOL pifAds1x1x_SetConfig(PIF_stAds1x1x *pstOwner, PIF_stAds1x1xConfig *pstConfig)
{
	pstOwner->__stConfig.usAll = pstConfig->usAll;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetConfig
 * @brief
 * @param pstOwner
 * @return
 */
PIF_stAds1x1xConfig pifAds1x1x_GetConfig(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig;
}

/**
 * @fn pifAds1x1x_SingleShotConvert
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifAds1x1x_SingleShotConvert(PIF_stAds1x1x *pstOwner)
{
	pstOwner->__stConfig.bt.OS_SSCS = 1;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_SetMux
 * @brief
 * @param pstOwner
 * @param enMux
 * @return
 */
BOOL pifAds1x1x_SetMux(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMux enMux)
{
	if (pstOwner->__unChannels == 1) return FALSE;

	pstOwner->__stConfig.bt.MUX = enMux;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetMux
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigMux pifAds1x1x_GetMux(PIF_stAds1x1x *pstOwner)
{
    return pstOwner->__stConfig.bt.MUX;
}

/**
 * @fn pifAds1x1x_SetGain
 * @brief
 * @param pstOwner
 * @param enPGA
 * @return
 */
BOOL pifAds1x1x_SetGain(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigPGA enPGA)
{
	if (pstOwner->_enType == AT_en1013 || pstOwner->_enType == AT_en1113) return FALSE;

	pstOwner->__stConfig.bt.PGA = enPGA;
    pstOwner->dConvertVoltage = _ConvertVoltage(pstOwner);
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetGain
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigPGA pifAds1x1x_GetGain(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.PGA;
}

/**
 * @fn pifAds1x1x_SetMode
 * @brief
 * @param pstOwner
 * @param enMode
 * @return
 */
BOOL pifAds1x1x_SetMode(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMode enMode)
{
	pstOwner->__stConfig.bt.MODE = enMode;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetMode
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigMode pifAds1x1x_GetMode(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.MODE;
}

/**
 * @fn pifAds1x1x_SetDataRate
 * @brief
 * @param pstOwner
 * @param enDR
 * @return
 */
BOOL pifAds1x1x_SetDataRate(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigDR enDR)
{
	pstOwner->__stConfig.bt.DR = enDR;
    pstOwner->__unConversionDelay = _ConversionDelay(pstOwner);
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetDataRate
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigDR pifAds1x1x_GetDataRate(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.DR;
}

/**
 * @fn pifAds1x1x_SetCompMode
 * @brief
 * @param pstOwner
 * @param enCompMode
 * @return
 */
BOOL pifAds1x1x_SetCompMode(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompMode enCompMode)
{
	if (pstOwner->_enType == AT_en1013 || pstOwner->_enType == AT_en1113) return FALSE;

	pstOwner->__stConfig.bt.COMP_MODE = enCompMode;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetCompMode
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigCompMode pifAds1x1x_GetCompMode(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.COMP_MODE;
}

/**
 * @fn pifAds1x1x_SetCompPol
 * @brief
 * @param pstOwner
 * @param enCompPol
 * @return
 */
BOOL pifAds1x1x_SetCompPol(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompPol enCompPol)
{
	if (pstOwner->_enType == AT_en1013 || pstOwner->_enType == AT_en1113) return FALSE;

	pstOwner->__stConfig.bt.COMP_POL = enCompPol;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetCompPol
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigCompPol pifAds1x1x_GetCompPol(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.COMP_POL;
}

/**
 * @fn pifAds1x1x_SetCompLat
 * @brief
 * @param pstOwner
 * @param enCompLat
 * @return
 */
BOOL pifAds1x1x_SetCompLat(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompLat enCompLat)
{
	if (pstOwner->_enType == AT_en1013 || pstOwner->_enType == AT_en1113) return FALSE;

	pstOwner->__stConfig.bt.COMP_LAT = enCompLat;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetCompLat
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigCompLat pifAds1x1x_GetCompLat(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.COMP_LAT;
}

/**
 * @fn pifAds1x1x_SetCompQue
 * @brief
 * @param pstOwner
 * @param enCompQue
 * @return
 */
BOOL pifAds1x1x_SetCompQue(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompQue enCompQue)
{
	if (pstOwner->_enType == AT_en1013 || pstOwner->_enType == AT_en1113) return FALSE;

	pstOwner->__stConfig.bt.COMP_QUE = enCompQue;
	return _WriteWord(pstOwner, AR_enCONFIG, pstOwner->__stConfig.usAll);
}

/**
 * @fn pifAds1x1x_GetCompQue
 * @brief
 * @param pstOwner
 * @return
 */
PIF_enAds1x1xConfigCompQue pifAds1x1x_GetCompQue(PIF_stAds1x1x *pstOwner)
{
	return pstOwner->__stConfig.bt.COMP_QUE;
}

/**
 * @fn pifAds1x1x_SetLoThresh
 * @brief
 * @param pstOwner
 * @param sThreshold
 * @return
 */
BOOL pifAds1x1x_SetLoThresh(PIF_stAds1x1x *pstOwner, int16_t sThreshold)
{
    int16_t v = (sThreshold & ((1 << pstOwner->__unResolution) - 1)) << pstOwner->__ucBitOffset;
    return _WriteWord(pstOwner, AR_enLO_THRESH, v);
}

/**
 * @fn pifAds1x1x_GetLoThresh
 * @brief
 * @param pstOwner
 * @return
 */
int16_t pifAds1x1x_GetLoThresh(PIF_stAds1x1x *pstOwner)
{
	uint16_t usData;

    _ReadWord(pstOwner, AR_enLO_THRESH, &usData);
    return usData >> pstOwner->__ucBitOffset;
}

/**
 * @fn pifAds1x1x_SetLoThreshVoltage
 * @brief
 * @param pstOwner
 * @param sThreshold
 * @return
 */
BOOL pifAds1x1x_SetLoThreshVoltage(PIF_stAds1x1x *pstOwner, double sThreshold)
{
    return pifAds1x1x_SetLoThresh(pstOwner, (int16_t)(sThreshold / pstOwner->dConvertVoltage));
}

/**
 * @fn pifAds1x1x_GetLoThreshVoltage
 * @brief
 * @param pstOwner
 * @return
 */
double pifAds1x1x_GetLoThreshVoltage(PIF_stAds1x1x *pstOwner)
{
    return pifAds1x1x_GetLoThresh(pstOwner) * pstOwner->dConvertVoltage;
}

/**
 * @fn pifAds1x1x_SetHiThresh
 * @brief
 * @param pstOwner
 * @param sThreshold
 * @return
 */
BOOL pifAds1x1x_SetHiThresh(PIF_stAds1x1x *pstOwner, int16_t sThreshold)
{
    int16_t v = (sThreshold & ((1 << pstOwner->__unResolution) - 1)) << pstOwner->__ucBitOffset;
    return _WriteWord(pstOwner, AR_enHI_THRESH, v);
}

/**
 * @fn pifAds1x1x_GetHiThresh
 * @brief
 * @param pstOwner
 * @return
 */
int16_t pifAds1x1x_GetHiThresh(PIF_stAds1x1x *pstOwner)
{
	uint16_t usData;

    _ReadWord(pstOwner, AR_enHI_THRESH, &usData);
    return usData >> pstOwner->__ucBitOffset;
}

/**
 * @fn pifAds1x1x_SetHiThreshVoltage
 * @brief
 * @param pstOwner
 * @param sThreshold
 * @return
 */
BOOL pifAds1x1x_SetHiThreshVoltage(PIF_stAds1x1x *pstOwner, double sThreshold)
{
    return pifAds1x1x_SetHiThresh(pstOwner, (int16_t)(sThreshold / pstOwner->dConvertVoltage));
}

/**
 * @fn pifAds1x1x_GetHiThreshVoltage
 * @brief
 * @param pstOwner
 * @return
 */
double pifAds1x1x_GetHiThreshVoltage(PIF_stAds1x1x *pstOwner)
{
    return pifAds1x1x_GetHiThresh(pstOwner) * pstOwner->dConvertVoltage;
}
