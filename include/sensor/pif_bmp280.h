#ifndef PIF_BMP280_H
#define PIF_BMP280_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_sensor_event.h"


#define BMP280_I2C_ADDR(N)		(0x76 + (N))

#define BMP280_WHO_AM_I_CONST	0x58


typedef enum EnPifBmp280Reg
{
	BMP280_REG_CALIB			= 0x88,
	BMP280_REG_ID				= 0xD0,
	BMP280_REG_RESET			= 0xE0,
	BMP280_REG_STATUS			= 0xF3,
	BMP280_REG_CTRL_MEAS		= 0xF4,
	BMP280_REG_CONFIG			= 0xF5,
	BMP280_REG_PRESS_MSB		= 0xF7,
	BMP280_REG_PRESS_LSB		= 0xF8,
	BMP280_REG_PRESS_XLSB		= 0xF9,
	BMP280_REG_TEMP_MSB			= 0xFA,
	BMP280_REG_TEMP_LSB			= 0xFB,
	BMP280_REG_TEMP_XLSB		= 0xFC
} PifBmp280Reg;


// Register : STATUS

#define BMP280_IM_UPDATE(N)		(N)
#define BMP280_MEASURING(N)		((N) << 3)

#define BMP280_IM_UPDATE_MASK	0x01
#define BMP280_MEASURING_MASK	0x08


// Register : CTRL_MEAS

typedef enum EnPifBmp280Mode
{
	BMP280_MODE_SLEEP		= 0,
	BMP280_MODE_FORCED		= 1,
	BMP280_OSRS_NORMAL		= 3
} PifBmp280Mode;

typedef enum EnPifBmp280OsrsP
{
	BMP280_OSRS_P_SKIP		= 0 << 2,
	BMP280_OSRS_P_X1		= 1 << 2,
	BMP280_OSRS_P_X2		= 2 << 2,
	BMP280_OSRS_P_X4		= 3 << 2,
	BMP280_OSRS_P_X8		= 4 << 2,
	BMP280_OSRS_P_X16		= 5 << 2
} PifBmp280OsrsP;

typedef enum EnPifBmp280OsrsT
{
	BMP280_OSRS_T_SKIP		= 0 << 5,
	BMP280_OSRS_T_X1		= 1 << 5,
	BMP280_OSRS_T_X2		= 2 << 5,
	BMP280_OSRS_T_X4		= 3 << 5,
	BMP280_OSRS_T_X8		= 4 << 5,
	BMP280_OSRS_T_X16		= 5 << 5
} PifBmp280OsrsT;

#define BMP280_MODE_MASK	0x03
#define BMP280_OSRS_P_MASK	0x1C
#define BMP280_OSRS_T_MASK	0xE0


// Register : CONFIG

#define BMP280_SPI3W_EN(N)		(N)

typedef enum EnPifBmp280Filter
{
	BMP280_FILTER_OFF			= 0 << 2,
	BMP280_FILTER_X2			= 1 << 2,
	BMP280_FILTER_X4			= 2 << 2,
	BMP280_FILTER_X8			= 3 << 2,
	BMP280_FILTER_X16			= 4 << 2
} PifBmp280Filter;

typedef enum EnPifBmp280TSB
{
	BMP280_T_SB_1_MS			= 0 << 5,
	BMP280_T_SB_63_MS			= 1 << 5,
	BMP280_T_SB_125_MS			= 2 << 5,
	BMP280_T_SB_250_MS			= 3 << 5,
	BMP280_T_SB_500_MS			= 4 << 5,
	BMP280_T_SB_1000_MS			= 5 << 5,
	BMP280_T_SB_2000_MS			= 6 << 5,
	BMP280_T_SB_4000_MS			= 7 << 5
} PifBmp280TSB;

#define BMP280_SPI3W_EN_MASK	0x01
#define BMP280_FILTER_MASK		0x1C
#define BMP280_T_SB_MASK		0xE0


typedef enum EnPifBmp280State
{
	BMP280_STATE_IDLE,
	BMP280_STATE_START,
	BMP280_STATE_WAIT,
	BMP280_STATE_READ,
	BMP280_STATE_CALCURATE
} PifBmp280State;


typedef struct StPifBmp280CalibParam 
{
    uint16_t dig_T1;	/* calibration T1 data */
    int16_t dig_T2;		/* calibration T2 data */
    int16_t dig_T3;		/* calibration T3 data */
    uint16_t dig_P1;	/* calibration P1 data */
    int16_t dig_P2;		/* calibration P2 data */
    int16_t dig_P3;		/* calibration P3 data */
    int16_t dig_P4;		/* calibration P4 data */
    int16_t dig_P5;		/* calibration P5 data */
    int16_t dig_P6;		/* calibration P6 data */
    int16_t dig_P7;		/* calibration P7 data */
    int16_t dig_P8;		/* calibration P8 data */
    int16_t dig_P9;		/* calibration P9 data */
    int32_t t_fine;		/* calibration t_fine data */
} PifBmp280CalibParam;


/**
 * @class StPifBmp280
 * @brief
 */
typedef struct StPifBmp280
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	union {
		PifI2cDevice* _p_i2c;
		PifSpiDevice* _p_spi;
	};
	uint8_t _osrs_p;
	uint8_t _osrs_t;
	PifTask* _p_task;

	// Private Member Variable
	PifBmp280CalibParam __calib_param;
	uint16_t __read_period;
	uint16_t __delay;
	PifBmp280State __state;
	uint32_t __start_time;
	int32_t __raw_pressure;
	int32_t __raw_temperature;

	// Read-only Function
	PifDeviceReg8Func _fn;

	// Private Event Function
	PifEvtBaroRead __evt_read;
} PifBmp280;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmp280_Config
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifBmp280_Config(PifBmp280* p_owner, PifId id);

/**
 * @fn pifBmp280_SetOverSamplingRate
 * @brief
 * @param p_owner
 * @param osrs_p
 * @param osrs_t
 */
void pifBmp280_SetOverSamplingRate(PifBmp280* p_owner, uint8_t osrs_p, uint8_t osrs_t);

/**
 * @fn pifBmp280_ReadRawData
 * @brief
 * @param p_owner
 * @param p_pressure
 * @param p_temperature
 * @return
 */
BOOL pifBmp280_ReadRawData(PifBmp280* p_owner, int32_t* p_pressure, int32_t* p_temperature);

/**
 * @fn pifBmp280_ReadBarometric
 * @brief
 * @param p_owner
 * @param p_pressure
 * @param p_temperature
 * @return
 */
BOOL pifBmp280_ReadBarometric(PifBmp280* p_owner, float* p_pressure, float* p_temperature);

/**
 * @fn pifBmp280_AttachTaskForReading
 * @brief
 * @param p_owner
 * @param id
 * @param read_period
 * @param evt_read
 * @param start
 * @return
 */
BOOL pifBmp280_AttachTaskForReading(PifBmp280* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_H
