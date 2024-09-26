#include "core/pif_task.h"
#include "sensor/pif_ads1x1x.h"


static double _convertVoltage(PifAds1x1x* p_owner)
{
    switch (p_owner->_config.bit.pga) {
        case ADS1X1X_PGA_FSR_6_144V: return 6.144 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_4_096V: return 4.096 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_2_048V: return 2.048 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_1_024V: return 1.024 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_0_512V: return 0.512 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_0_256V: return 0.256 / (0x7FFF >> p_owner->__bit_offset);
    }
    return 2.048 / (0x7FFF >> p_owner->__bit_offset);
}

static uint32_t _conversionDelay(PifAds1x1x* p_owner)
{
	uint16_t data_rate = 0;
	uint32_t delay = 0;

    if (p_owner->__resolution == 12) {
        switch (p_owner->_config.bit.dr) {
            case ADS1X1X_DR_12B_0128_SPS: data_rate = 128; break;
            case ADS1X1X_DR_12B_0250_SPS: data_rate = 250; break;
            case ADS1X1X_DR_12B_0490_SPS: data_rate = 490; break;
            case ADS1X1X_DR_12B_0920_SPS: data_rate = 920; break;
            case ADS1X1X_DR_12B_1600_SPS: data_rate = 1600; break;
            case ADS1X1X_DR_12B_2400_SPS: data_rate = 2400; break;
            case ADS1X1X_DR_12B_3300_SPS: data_rate = 3300; break;
        }
    }
    else {
        switch (p_owner->_config.bit.dr) {
            case ADS1X1X_DR_16B_0008_SPS: data_rate = 8; break;
            case ADS1X1X_DR_16B_0016_SPS: data_rate = 16; break;
            case ADS1X1X_DR_16B_0032_SPS: data_rate = 32; break;
            case ADS1X1X_DR_16B_0064_SPS: data_rate = 64; break;
            case ADS1X1X_DR_16B_0128_SPS: data_rate = 128; break;
            case ADS1X1X_DR_16B_0250_SPS: data_rate = 250; break;
            case ADS1X1X_DR_16B_0475_SPS: data_rate = 475; break;
            case ADS1X1X_DR_16B_0860_SPS: data_rate = 860; break;
        }
    }
    if (data_rate) {
		delay = (1000000UL - 1) / data_rate + 1;
    }
    return delay;
}

BOOL pifAds1x1x_Init(PifAds1x1x* p_owner, PifId id, PifAds1x1xType type, PifI2cPort* p_port, uint8_t addr)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifAds1x1x));

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->_p_i2c = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr);
    if (!p_owner->_p_i2c) return FALSE;

    switch (type) {
    case ADS1X1X_TYPE_1115: p_owner->__resolution = 16; p_owner->__channels = 4; break;
    case ADS1X1X_TYPE_1114: p_owner->__resolution = 16; p_owner->__channels = 1; break;
    case ADS1X1X_TYPE_1113: p_owner->__resolution = 16; p_owner->__channels = 1; break;
    case ADS1X1X_TYPE_1015: p_owner->__resolution = 12; p_owner->__channels = 4; break;
    case ADS1X1X_TYPE_1014: p_owner->__resolution = 12; p_owner->__channels = 1; break;
    case ADS1X1X_TYPE_1013: p_owner->__resolution = 12; p_owner->__channels = 1; break;
    default:
		pif_error = E_INVALID_PARAM;
    	goto fail;
    }
    p_owner->_type = type;
    p_owner->__bit_offset = p_owner->__resolution == 12 ? 4 : 0;
    if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, &p_owner->_config.word)) goto fail;
    p_owner->convert_voltage = _convertVoltage(p_owner);
    p_owner->__conversion_delay = _conversionDelay(p_owner);
    return TRUE;

fail:
	pifAds1x1x_Clear(p_owner);
	return FALSE;
}

void pifAds1x1x_Clear(PifAds1x1x* p_owner)
{
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}

int16_t pifAds1x1x_Read(PifAds1x1x* p_owner)
{
	uint16_t data;

	if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONVERSION, &data)) return 0;
	return data >> p_owner->__bit_offset;
}

int16_t pifAds1x1x_ReadMux(PifAds1x1x* p_owner, PifAds1x1xMux mux)
{
	uint16_t data;
	PifAds1x1xConfig config;

	if (p_owner->__channels == 1 || p_owner->_config.bit.mode == ADS1X1X_MODE_CONTINUOUS) return 0;

	p_owner->_config.bit.mux = mux;
	config.word = p_owner->_config.word;
	config.bit.os_sscs = 1;
	if (!pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, config.word)) return 0;
	if (p_owner->__conversion_delay) {
		pifTaskManager_YieldUs(p_owner->__conversion_delay);
	}
	if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONVERSION, &data)) return 0;
	return data >> p_owner->__bit_offset;
}

double pifAds1x1x_Voltage(PifAds1x1x* p_owner)
{
    return (double)pifAds1x1x_Read(p_owner) * p_owner->convert_voltage;
}

double pifAds1x1x_VoltageMux(PifAds1x1x* p_owner, PifAds1x1xMux mux)
{
    return (double)pifAds1x1x_ReadMux(p_owner, mux) * p_owner->convert_voltage;
}

BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, PifAds1x1xConfig* p_config)
{
	p_owner->_config.word = p_config->word;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SingleShotConvert(PifAds1x1x* p_owner)
{
	p_owner->_config.bit.os_sscs = 1;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetMux(PifAds1x1x* p_owner, PifAds1x1xMux mux)
{
	if (p_owner->__channels == 1) return FALSE;

	p_owner->_config.bit.mux = mux;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetGain(PifAds1x1x* p_owner, PifAds1x1xPGA pga)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	p_owner->_config.bit.pga = pga;
    p_owner->convert_voltage = _convertVoltage(p_owner);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetMode(PifAds1x1x* p_owner, PifAds1x1xMode mode)
{
	p_owner->_config.bit.mode = mode;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetDataRate(PifAds1x1x* p_owner, PifAds1x1xDR dr)
{
	p_owner->_config.bit.dr = dr;
    p_owner->__conversion_delay = _conversionDelay(p_owner);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetCompMode(PifAds1x1x* p_owner, PifAds1x1xCompMode comp_mode)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	p_owner->_config.bit.comp_mode = comp_mode;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetCompPol(PifAds1x1x* p_owner, PifAds1x1xCompPol comp_pol)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	p_owner->_config.bit.comp_pol = comp_pol;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetCompLat(PifAds1x1x* p_owner, PifAds1x1xCompLat comp_lat)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	p_owner->_config.bit.comp_lat = comp_lat;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetCompQue(PifAds1x1x* p_owner, PifAds1x1xCompQue comp_que)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	p_owner->_config.bit.comp_que = comp_que;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config.word);
}

BOOL pifAds1x1x_SetLoThresh(PifAds1x1x* p_owner, int16_t threshold)
{
    int16_t v = (threshold & ((1 << p_owner->__resolution) - 1)) << p_owner->__bit_offset;
    return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_LO_THRESH, v);
}

int16_t pifAds1x1x_GetLoThresh(PifAds1x1x* p_owner)
{
	uint16_t data;

    if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_LO_THRESH, &data)) return 0;
    return data >> p_owner->__bit_offset;
}

BOOL pifAds1x1x_SetLoThreshVoltage(PifAds1x1x* p_owner, double threshold)
{
    return pifAds1x1x_SetLoThresh(p_owner, (int16_t)(threshold / p_owner->convert_voltage));
}

double pifAds1x1x_GetLoThreshVoltage(PifAds1x1x* p_owner)
{
    return pifAds1x1x_GetLoThresh(p_owner) * p_owner->convert_voltage;
}

BOOL pifAds1x1x_SetHiThresh(PifAds1x1x* p_owner, int16_t threshold)
{
    int16_t v = (threshold & ((1 << p_owner->__resolution) - 1)) << p_owner->__bit_offset;
    return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_HI_THRESH, v);
}

int16_t pifAds1x1x_GetHiThresh(PifAds1x1x* p_owner)
{
	uint16_t data;

    if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_HI_THRESH, &data)) return 0;
    return data >> p_owner->__bit_offset;
}

BOOL pifAds1x1x_SetHiThreshVoltage(PifAds1x1x* p_owner, double threshold)
{
    return pifAds1x1x_SetHiThresh(p_owner, (int16_t)(threshold / p_owner->convert_voltage));
}

double pifAds1x1x_GetHiThreshVoltage(PifAds1x1x* p_owner)
{
    return pifAds1x1x_GetHiThresh(p_owner) * p_owner->convert_voltage;
}
