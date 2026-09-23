#include "core/pif_task_manager.h"
#include "sensor/pif_ads1x1x.h"


/**
 * @fn _convertVoltage
 * @brief Converts the latest sample from convert voltage into a voltage value.
 * @param p_owner Pointer to the owner instance.
 * @return Computed floating-point value.
 */
static double _convertVoltage(PifAds1x1x* p_owner)
{
    switch (p_owner->_config & ADS1X1X_PGA_MASK) {
        case ADS1X1X_PGA_FSR_6_144V: return 6.144 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_4_096V: return 4.096 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_2_048V: return 2.048 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_1_024V: return 1.024 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_0_512V: return 0.512 / (0x7FFF >> p_owner->__bit_offset);
        case ADS1X1X_PGA_FSR_0_256V: return 0.256 / (0x7FFF >> p_owner->__bit_offset);
    }
    return 2.048 / (0x7FFF >> p_owner->__bit_offset);
}

/**
 * @fn _conversionDelay
 * @brief Internal helper that supports conversion delay logic.
 * @param p_owner Pointer to the owner instance.
 * @return Computed integer value.
 */
static uint32_t _conversionDelay(PifAds1x1x* p_owner)
{
	uint16_t data_rate = 0;
	uint32_t delay = 0;

    if (p_owner->__resolution == 12) {
        switch (p_owner->_config & ADS1X1X_DR_MASK) {
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
        switch (p_owner->_config & ADS1X1X_DR_MASK) {
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

/**
 * @fn _conversionTicks
 * @brief The conversion time of the current data rate, in ticks of the timer manager.
 * @param p_owner Pointer to the owner instance.
 * @return Ticks to wait.
 */
static uint32_t _conversionTicks(PifAds1x1x* p_owner)
{
	uint32_t period = p_owner->__p_timer_manager->_period1us;

	// Rounded up: a tick too many leaves the sample sitting in the device a little longer, which
	// costs nothing, while a tick too few would read the previous sample instead.
	// It cannot round down to zero, which pifTimer_Start() would reject, because only a conversion
	// longer than PIF_ADS1X1X_SPIN_LIMIT_US is timed at all and the rest are waited out.
	return (p_owner->__conversion_delay + period - 1) / period;
}

/**
 * @fn _readConversion
 * @brief Reads the conversion that has finished into _value. Handing the sample on is left to the
 *        caller, which is what keeps the waited path from having to call the read event at all.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE when the conversion register was read, otherwise FALSE.
 */
static BOOL _readConversion(PifAds1x1x* p_owner)
{
	uint16_t data;

	p_owner->_state = ADS1X1X_STATE_IDLE;
	if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONVERSION, &data)) return FALSE;

	p_owner->_value = (int16_t)(data >> p_owner->__bit_offset);
	return TRUE;
}

/**
 * @fn _evtTimerFinish
 * @brief Reads the conversion the timer was waiting out and hands the sample over.
 * @param p_issuer Issuer pointer castable to PifAds1x1x.
 */
static void _evtTimerFinish(PifIssuerP p_issuer)
{
	PifAds1x1x* p_owner = (PifAds1x1x*)p_issuer;
	BOOL result;

	// Reached from the timer process of the task manager, which runs at the start of a loop with
	// the CPU to itself, so the transfer below is in the same context as one inside any task.
	result = _readConversion(p_owner);

	// Reported either way. Saying nothing when the transfer fails would leave a caller that waits
	// on the event waiting for a sample that is never coming.
	if (p_owner->__evt_read) (*p_owner->__evt_read)(p_owner, result, p_owner->_value);
}

BOOL pifAds1x1x_Init(PifAds1x1x* p_owner, PifId id, PifAds1x1xType type, PifI2cPort* p_port, uint8_t addr, void *p_client)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifAds1x1x));

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->_p_i2c = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr, p_client);
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
    if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, &p_owner->_config)) goto fail;
    p_owner->convert_voltage = _convertVoltage(p_owner);
    p_owner->__conversion_delay = _conversionDelay(p_owner);
    return TRUE;

fail:
	pifAds1x1x_Clear(p_owner);
	return FALSE;
}

void pifAds1x1x_Clear(PifAds1x1x* p_owner)
{
	pifAds1x1x_DetachTimer(p_owner);
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

BOOL pifAds1x1x_AttachTimer(PifAds1x1x* p_owner, PifTimerManager* p_timer_manager, PifEvtAds1x1xRead evt_read)
{
	if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->__p_timer) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
	if (!p_owner->__p_timer) return FALSE;

	// Not pifTimer_AttachEvtIntFinish(): that one is called from inside
	// pifTimerManager_sigTick(), and reading the conversion is an I2C transfer that has no
	// business in a tick interrupt.
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner);

	p_owner->__p_timer_manager = p_timer_manager;
	p_owner->__evt_read = evt_read;
	p_owner->_state = ADS1X1X_STATE_IDLE;
	return TRUE;
}

void pifAds1x1x_DetachTimer(PifAds1x1x* p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
	p_owner->__p_timer_manager = NULL;
	p_owner->_state = ADS1X1X_STATE_IDLE;
}

PifAds1x1xStart pifAds1x1x_StartMux(PifAds1x1x* p_owner, PifAds1x1xMux mux)
{
	uint16_t config;

	if (!p_owner->__p_timer) {
		pif_error = E_CANNOT_FOUND;
		return ADS1X1X_START_FAILURE;
	}
	// A single channel part has no multiplexer to point anywhere, and in continuous mode the device
	// converts on its own: there is no conversion to start and no wait to time.
	if (p_owner->__channels == 1 || (p_owner->_config & ADS1X1X_MODE_MASK) == ADS1X1X_MODE_CONTINUOUS) {
		pif_error = E_INVALID_STATE;
		return ADS1X1X_START_FAILURE;
	}
	// There is one conversion register and one timer, so the one on its way has to be collected
	// before the next may start.
	if (p_owner->_state != ADS1X1X_STATE_IDLE) {
		pif_error = E_INVALID_STATE;
		return ADS1X1X_START_FAILURE;
	}

	SET_BIT_FILED(p_owner->_config, ADS1X1X_MUX_MASK, mux);
	// The bit that starts the conversion is written to the device but kept out of _config, which
	// describes the configuration rather than the one shot that used it.
	config = p_owner->_config;
	config |= ADS1X1X_SSCS_SINGLE;
	if (!pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, config)) return ADS1X1X_START_FAILURE;

	// Too short for the timer to express, so it is waited out here. The answer goes back through
	// the return value and _value rather than through the read event: an event called from inside
	// this function could start the next conversion from within it, and a scan driven that way
	// would nest one call per conversion. The wait is bounded by PIF_ADS1X1X_SPIN_LIMIT_US, which
	// is less than the tick the timer would have rounded it up to.
	if (p_owner->__conversion_delay <= PIF_ADS1X1X_SPIN_LIMIT_US) {
		pif_Delay1us(p_owner->__conversion_delay);
		if (!_readConversion(p_owner)) return ADS1X1X_START_FAILURE;
		return ADS1X1X_START_DONE;
	}

	p_owner->_state = ADS1X1X_STATE_CONVERT;
	if (!pifTimer_Start(p_owner->__p_timer, _conversionTicks(p_owner))) {
		p_owner->_state = ADS1X1X_STATE_IDLE;
		return ADS1X1X_START_FAILURE;
	}
	return ADS1X1X_START_TIMED;
}

double pifAds1x1x_Voltage(PifAds1x1x* p_owner)
{
    return (double)pifAds1x1x_Read(p_owner) * p_owner->convert_voltage;
}

BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, uint16_t config)
{
	p_owner->_config = config;
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SingleShotConvert(PifAds1x1x* p_owner)
{
	SET_BIT_FILED(p_owner->_config, ADS1X1X_OS_SSCS_MASK, ADS1X1X_SSCS_SINGLE);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetMux(PifAds1x1x* p_owner, PifAds1x1xMux mux)
{
	if (p_owner->__channels == 1) return FALSE;

	SET_BIT_FILED(p_owner->_config, ADS1X1X_MUX_MASK, mux);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetGain(PifAds1x1x* p_owner, PifAds1x1xPGA pga)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	SET_BIT_FILED(p_owner->_config, ADS1X1X_PGA_MASK, pga);
    p_owner->convert_voltage = _convertVoltage(p_owner);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetMode(PifAds1x1x* p_owner, PifAds1x1xMode mode)
{
	SET_BIT_FILED(p_owner->_config, ADS1X1X_MODE_MASK, mode);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetDataRate(PifAds1x1x* p_owner, PifAds1x1xDR dr)
{
	SET_BIT_FILED(p_owner->_config, ADS1X1X_DR_MASK, dr);
    p_owner->__conversion_delay = _conversionDelay(p_owner);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetCompMode(PifAds1x1x* p_owner, PifAds1x1xCompMode comp_mode)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	SET_BIT_FILED(p_owner->_config, ADS1X1X_COMP_MODE_MASK, comp_mode);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetCompPol(PifAds1x1x* p_owner, PifAds1x1xCompPol comp_pol)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	SET_BIT_FILED(p_owner->_config, ADS1X1X_COMP_POL_MASK, comp_pol);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetCompLat(PifAds1x1x* p_owner, PifAds1x1xCompLat comp_lat)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	SET_BIT_FILED(p_owner->_config, ADS1X1X_COMP_LAT_MASK, comp_lat);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
}

BOOL pifAds1x1x_SetCompQue(PifAds1x1x* p_owner, PifAds1x1xCompQue comp_que)
{
	if (p_owner->_type == ADS1X1X_TYPE_1013 || p_owner->_type == ADS1X1X_TYPE_1113) return FALSE;

	SET_BIT_FILED(p_owner->_config, ADS1X1X_COMP_QUE_MASK, comp_que);
	return pifI2cDevice_WriteRegWord(p_owner->_p_i2c, ADS1X1X_REG_CONFIG, p_owner->_config);
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
