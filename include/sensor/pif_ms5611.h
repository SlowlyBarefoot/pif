#ifndef PIF_MS5611_H
#define PIF_MS5611_H


#include "core/pif_i2c.h"
#include "core/pif_task.h"


#define	MS5611_I2C_ADDR(N)		(0x76 + (N))


typedef enum EnPifMs5611Reg
{
	MS5611_REG_ADC_READ			= 0x00,
	MS5611_REG_RESET			= 0x1E,
	MS5611_REG_CONV_D1			= 0x40,		// Pressure
	MS5611_REG_CONV_D2			= 0x50,		// Temperature
	MS5611_REG_READ_PROM		= 0xA0
} PifMs5611Reg;

typedef enum EnPifMs5611Osr
{
	MS5611_OSR_256				= 0x00,
	MS5611_OSR_512				= 0x02,
	MS5611_OSR_1024				= 0x04,		// Default
	MS5611_OSR_2048				= 0x06,
	MS5611_OSR_4096				= 0x08
} PifMs5611Osr;

typedef enum EnPifMs5611State
{
	MS5611_STATE_IDLE,
	MS5611_STATE_TEMPERATURE_START,
	MS5611_STATE_TEMPERATURE_WAIT,
	MS5611_STATE_PRESSURE_START,
	MS5611_STATE_PRESSURE_WAIT,
	MS5611_STATE_CALCURATE
} PifMs5611State;


typedef void (*PifEvtMs5611Read)(int32_t pressure, float temperature);


/**
 * @class StPifMs5611
 * @brief
 */
typedef struct StPifMs5611
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;
	uint16_t _prom[8];
	uint16_t _over_sampling_rate;
	uint8_t _conversion_time;
	PifTask* _p_task;

	// Private Member Variable
	uint16_t __read_period;
	PifMs5611State __state;
	uint32_t __D1, __D2;

	// Private Event Function
	PifEvtMs5611Read __evt_read;
} PifMs5611;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMs5611_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifMs5611_Init(PifMs5611* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifMs5611_Clear
 * @brief
 * @param p_owner
 */
void pifMs5611_Clear(PifMs5611* p_owner);

/**
 * @fn pifMs5611_SetOverSamplingRate
 * @brief
 * @param p_owner
 * @param osr
 */
void pifMs5611_SetOverSamplingRate(PifMs5611* p_owner, uint16_t osr);

/**
 * @fn pifMs5611_ReadRawTemperature
 * @brief
 * @param p_owner
 * @param p_data
 * @return
 */
BOOL pifMs5611_ReadRawTemperature(PifMs5611* p_owner, uint32_t* p_data);

/**
 * @fn pifMs5611_ReadRawPressure
 * @brief
 * @param p_owner
 * @param p_data
 * @return
 */
BOOL pifMs5611_ReadRawPressure(PifMs5611* p_owner, uint32_t* p_data);

/**
 * @fn pifMs5611_ReadBarometric
 * @brief
 * @param p_owner
 * @param p_pressure
 * @param p_temperature
 * @return
 */
BOOL pifMs5611_ReadBarometric(PifMs5611* p_owner, int32_t* p_pressure, float* p_temperature);

/**
 * @fn pifMs5611_AddTaskForReading
 * @brief
 * @param p_owner
 * @param read_period
 * @param evt_read
 * @return
 */
BOOL pifMs5611_AddTaskForReading(PifMs5611* p_owner, uint16_t read_period, PifEvtMs5611Read evt_read);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MS5611_H
