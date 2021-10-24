#include "pif_ads1x1x.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif


#define DEFAULT_I2C_ADDR	0x48


static BOOL _readWord(PifAds1x1x* p_owner, const PifAds1x1xReg reg, uint16_t* p_data)
{
	p_owner->_i2c.p_data[0] = reg;
	if (!pifI2c_Write(&p_owner->_i2c, 1)) return FALSE;
	if (!pifI2c_Read(&p_owner->_i2c, 2)) return FALSE;
	*p_data = (p_owner->_i2c.p_data[0] << 8) + p_owner->_i2c.p_data[1];
	return TRUE;
}

static BOOL _writeWord(PifAds1x1x* p_owner, const PifAds1x1xReg reg, uint16_t data)
{
	p_owner->_i2c.p_data[0] = reg;
	p_owner->_i2c.p_data[1] = data >> 8;
	p_owner->_i2c.p_data[2] = data & 0xFF;
	return pifI2c_Write(&p_owner->_i2c, 3);
}

static double _convertVoltage(PifAds1x1x* p_owner)
{
    switch (p_owner->__config.bt.pga) {
        case ACP_FSR_6_144V: return 6.144 / (0x7FFF >> p_owner->__bit_offset);
        case ACP_FSR_4_096V: return 4.096 / (0x7FFF >> p_owner->__bit_offset);
        case ACP_FSR_2_048V: return 2.048 / (0x7FFF >> p_owner->__bit_offset);
        case ACP_FSR_1_024V: return 1.024 / (0x7FFF >> p_owner->__bit_offset);
        case ACP_FSR_0_512V: return 0.512 / (0x7FFF >> p_owner->__bit_offset);
        case ACP_FSR_0_256V: return 0.256 / (0x7FFF >> p_owner->__bit_offset);
    }
    return 2.048 / (0x7FFF >> p_owner->__bit_offset);
}

static uint32_t _conversionDelay(PifAds1x1x* p_owner)
{
	uint16_t data_rate = 0;

    if (p_owner->__resolution == 12) {
        switch (p_owner->__config.bt.dr) {
            case ACD_DR_12B_0128_SPS: data_rate = 128; break;
            case ACD_DR_12B_0250_SPS: data_rate = 250; break;
            case ACD_DR_12B_0490_SPS: data_rate = 490; break;
            case ACD_DR_12B_0920_SPS: data_rate = 920; break;
            case ACD_DR_12B_1600_SPS: data_rate = 1600; break;
            case ACD_DR_12B_2400_SPS: data_rate = 2400; break;
            case ACD_DR_12B_3300_SPS: data_rate = 3300; break;
        }
    }
    else {
        switch (p_owner->__config.bt.dr) {
            case ACD_DR_16B_0008_SPS: data_rate = 8; break;
            case ACD_DR_16B_0016_SPS: data_rate = 16; break;
            case ACD_DR_16B_0032_SPS: data_rate = 32; break;
            case ACD_DR_16B_0064_SPS: data_rate = 64; break;
            case ACD_DR_16B_0128_SPS: data_rate = 128; break;
            case ACD_DR_16B_0250_SPS: data_rate = 250; break;
            case ACD_DR_16B_0475_SPS: data_rate = 475; break;
            case ACD_DR_16B_0860_SPS: data_rate = 860; break;
        }
    }
    if (data_rate) {
		if (pif_act_timer1us) {
			return (1000000UL - 1) / data_rate + 1;
		}
		else {
			return (1000UL - 1) / data_rate + 1;
		}
    }
    return 0;
}

/**
 * @fn pifAds1x1x_Create
 * @brief
 * @param id
 * @param type
 * @return
 */
PifAds1x1x* pifAds1x1x_Create(PifId id, PifAds1x1xType type)
{
    PifAds1x1x *p_owner = NULL;

    p_owner = calloc(sizeof(PifAds1x1x), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

    if (!pifI2c_Init(&p_owner->_i2c, id, 4)) return NULL;

    p_owner->_i2c.addr = DEFAULT_I2C_ADDR;
    switch (type) {
    case AT_1115: p_owner->__resolution = 16; p_owner->__channels = 4; break;
    case AT_1114: p_owner->__resolution = 16; p_owner->__channels = 1; break;
    case AT_1113: p_owner->__resolution = 16; p_owner->__channels = 1; break;
    case AT_1015: p_owner->__resolution = 12; p_owner->__channels = 4; break;
    case AT_1014: p_owner->__resolution = 12; p_owner->__channels = 1; break;
    case AT_1013: p_owner->__resolution = 12; p_owner->__channels = 1; break;
    default:
		pif_error = E_INVALID_PARAM;
    	return NULL;
    }
    p_owner->_type = type;
    p_owner->__bit_offset = p_owner->__resolution == 12 ? 4 : 0;
    _readWord(p_owner, AR_CONFIG, &p_owner->__config.all);
    p_owner->convert_voltage = _convertVoltage(p_owner);
    p_owner->__conversion_delay = _conversionDelay(p_owner);
    return p_owner;
}

/**
 * @fn pifAds1x1x_Destroy
 * @brief
 * @param pp_owner
 */
void pifAds1x1x_Destroy(PifAds1x1x** pp_owner)
{
    if (*pp_owner) {
    	if ((*pp_owner)->_i2c.p_data) {
        	free((*pp_owner)->_i2c.p_data);
        	(*pp_owner)->_i2c.p_data = NULL;
    	}
    	free(*pp_owner);
    	*pp_owner = NULL;
    }
}

/**
 * @fn pifAds1x1x_SetAddress
 * @brief
 * @param p_owner
 * @param addr
 */
void pifAds1x1x_SetAddress(PifAds1x1x* p_owner, uint8_t addr)
{
	p_owner->_i2c.addr = addr;
}

/**
 * @fn pifAds1x1x_Read
 * @brief
 * @param p_owner
 * @return
 */
int16_t pifAds1x1x_Read(PifAds1x1x* p_owner)
{
	uint16_t data;

	if (!_readWord(p_owner, AR_CONVERSION, &data)) return 0;
	return data >> p_owner->__bit_offset;
}

/**
 * @fn pifAds1x1x_ReadMux
 * @brief
 * @param p_owner
 * @param mux
 * @return
 */
int16_t pifAds1x1x_ReadMux(PifAds1x1x* p_owner, PifAds1x1xConfigMux mux)
{
	uint16_t data;
	PifAds1x1xConfig config;

	if (p_owner->__channels == 1 || p_owner->__config.bt.mode == ACM_CONTINUOUS) return 0;

	p_owner->__config.bt.mux = mux;
	config.all = p_owner->__config.all;
	config.bt.os_sscs = 1;
	if (!_writeWord(p_owner, AR_CONFIG, config.all)) return 0;
	if (p_owner->__conversion_delay) {
		if (pif_act_timer1us) {
			pifTaskManager_YieldUs(p_owner->__conversion_delay);
		}
		else {
			pifTaskManager_YieldMs(p_owner->__conversion_delay);
		}
	}
	if (!_readWord(p_owner, AR_CONVERSION, &data)) return 0;
	return data >> p_owner->__bit_offset;
}

/**
 * @fn pifAds1x1x_Voltage
 * @brief
 * @param p_owner
 * @return
 */
double pifAds1x1x_Voltage(PifAds1x1x* p_owner)
{
    return (double)pifAds1x1x_Read(p_owner) * p_owner->convert_voltage;
}

/**
 * @fn pifAds1x1x_VoltageMux
 * @brief
 * @param p_owner
 * @param mux
 * @return
 */
double pifAds1x1x_VoltageMux(PifAds1x1x* p_owner, PifAds1x1xConfigMux mux)
{
    return (double)pifAds1x1x_ReadMux(p_owner, mux) * p_owner->convert_voltage;
}

/**
 * @fn pifAds1x1x_SetConfig
 * @brief
 * @param p_owner
 * @param p_config
 * @return
 */
BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, PifAds1x1xConfig* p_config)
{
	p_owner->__config.all = p_config->all;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetConfig
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfig pifAds1x1x_GetConfig(PifAds1x1x* p_owner)
{
	return p_owner->__config;
}

/**
 * @fn pifAds1x1x_SingleShotConvert
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifAds1x1x_SingleShotConvert(PifAds1x1x* p_owner)
{
	p_owner->__config.bt.os_sscs = 1;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_SetMux
 * @brief
 * @param p_owner
 * @param mux
 * @return
 */
BOOL pifAds1x1x_SetMux(PifAds1x1x* p_owner, PifAds1x1xConfigMux mux)
{
	if (p_owner->__channels == 1) return FALSE;

	p_owner->__config.bt.mux = mux;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetMux
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigMux pifAds1x1x_GetMux(PifAds1x1x* p_owner)
{
    return p_owner->__config.bt.mux;
}

/**
 * @fn pifAds1x1x_SetGain
 * @brief
 * @param p_owner
 * @param pga
 * @return
 */
BOOL pifAds1x1x_SetGain(PifAds1x1x* p_owner, PifAds1x1xConfigPGA pga)
{
	if (p_owner->_type == AT_1013 || p_owner->_type == AT_1113) return FALSE;

	p_owner->__config.bt.pga = pga;
    p_owner->convert_voltage = _convertVoltage(p_owner);
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetGain
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigPGA pifAds1x1x_GetGain(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.pga;
}

/**
 * @fn pifAds1x1x_SetMode
 * @brief
 * @param p_owner
 * @param mode
 * @return
 */
BOOL pifAds1x1x_SetMode(PifAds1x1x* p_owner, PifAds1x1xConfigMode mode)
{
	p_owner->__config.bt.mode = mode;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetMode
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigMode pifAds1x1x_GetMode(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.mode;
}

/**
 * @fn pifAds1x1x_SetDataRate
 * @brief
 * @param p_owner
 * @param dr
 * @return
 */
BOOL pifAds1x1x_SetDataRate(PifAds1x1x* p_owner, PifAds1x1xConfigDR dr)
{
	p_owner->__config.bt.dr = dr;
    p_owner->__conversion_delay = _conversionDelay(p_owner);
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetDataRate
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigDR pifAds1x1x_GetDataRate(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.dr;
}

/**
 * @fn pifAds1x1x_SetCompMode
 * @brief
 * @param p_owner
 * @param comp_mode
 * @return
 */
BOOL pifAds1x1x_SetCompMode(PifAds1x1x* p_owner, PifAds1x1xConfigCompMode comp_mode)
{
	if (p_owner->_type == AT_1013 || p_owner->_type == AT_1113) return FALSE;

	p_owner->__config.bt.comp_mode = comp_mode;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetCompMode
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompMode pifAds1x1x_GetCompMode(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.comp_mode;
}

/**
 * @fn pifAds1x1x_SetCompPol
 * @brief
 * @param p_owner
 * @param comp_pol
 * @return
 */
BOOL pifAds1x1x_SetCompPol(PifAds1x1x* p_owner, PifAds1x1xConfigCompPol comp_pol)
{
	if (p_owner->_type == AT_1013 || p_owner->_type == AT_1113) return FALSE;

	p_owner->__config.bt.comp_pol = comp_pol;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetCompPol
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompPol pifAds1x1x_GetCompPol(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.comp_pol;
}

/**
 * @fn pifAds1x1x_SetCompLat
 * @brief
 * @param p_owner
 * @param comp_lat
 * @return
 */
BOOL pifAds1x1x_SetCompLat(PifAds1x1x* p_owner, PifAds1x1xConfigCompLat comp_lat)
{
	if (p_owner->_type == AT_1013 || p_owner->_type == AT_1113) return FALSE;

	p_owner->__config.bt.comp_lat = comp_lat;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetCompLat
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompLat pifAds1x1x_GetCompLat(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.comp_lat;
}

/**
 * @fn pifAds1x1x_SetCompQue
 * @brief
 * @param p_owner
 * @param comp_que
 * @return
 */
BOOL pifAds1x1x_SetCompQue(PifAds1x1x* p_owner, PifAds1x1xConfigCompQue comp_que)
{
	if (p_owner->_type == AT_1013 || p_owner->_type == AT_1113) return FALSE;

	p_owner->__config.bt.comp_que = comp_que;
	return _writeWord(p_owner, AR_CONFIG, p_owner->__config.all);
}

/**
 * @fn pifAds1x1x_GetCompQue
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompQue pifAds1x1x_GetCompQue(PifAds1x1x* p_owner)
{
	return p_owner->__config.bt.comp_que;
}

/**
 * @fn pifAds1x1x_SetLoThresh
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetLoThresh(PifAds1x1x* p_owner, int16_t threshold)
{
    int16_t v = (threshold & ((1 << p_owner->__resolution) - 1)) << p_owner->__bit_offset;
    return _writeWord(p_owner, AR_LO_THRESH, v);
}

/**
 * @fn pifAds1x1x_GetLoThresh
 * @brief
 * @param p_owner
 * @return
 */
int16_t pifAds1x1x_GetLoThresh(PifAds1x1x* p_owner)
{
	uint16_t data;

    _readWord(p_owner, AR_LO_THRESH, &data);
    return data >> p_owner->__bit_offset;
}

/**
 * @fn pifAds1x1x_SetLoThreshVoltage
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetLoThreshVoltage(PifAds1x1x* p_owner, double threshold)
{
    return pifAds1x1x_SetLoThresh(p_owner, (int16_t)(threshold / p_owner->convert_voltage));
}

/**
 * @fn pifAds1x1x_GetLoThreshVoltage
 * @brief
 * @param p_owner
 * @return
 */
double pifAds1x1x_GetLoThreshVoltage(PifAds1x1x* p_owner)
{
    return pifAds1x1x_GetLoThresh(p_owner) * p_owner->convert_voltage;
}

/**
 * @fn pifAds1x1x_SetHiThresh
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetHiThresh(PifAds1x1x* p_owner, int16_t threshold)
{
    int16_t v = (threshold & ((1 << p_owner->__resolution) - 1)) << p_owner->__bit_offset;
    return _writeWord(p_owner, AR_HI_THRESH, v);
}

/**
 * @fn pifAds1x1x_GetHiThresh
 * @brief
 * @param p_owner
 * @return
 */
int16_t pifAds1x1x_GetHiThresh(PifAds1x1x* p_owner)
{
	uint16_t data;

    _readWord(p_owner, AR_HI_THRESH, &data);
    return data >> p_owner->__bit_offset;
}

/**
 * @fn pifAds1x1x_SetHiThreshVoltage
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetHiThreshVoltage(PifAds1x1x* p_owner, double threshold)
{
    return pifAds1x1x_SetHiThresh(p_owner, (int16_t)(threshold / p_owner->convert_voltage));
}

/**
 * @fn pifAds1x1x_GetHiThreshVoltage
 * @brief
 * @param p_owner
 * @return
 */
double pifAds1x1x_GetHiThreshVoltage(PifAds1x1x* p_owner)
{
    return pifAds1x1x_GetHiThresh(p_owner) * p_owner->convert_voltage;
}
