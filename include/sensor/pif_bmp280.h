#ifndef PIF_BMP280_H
#define PIF_BMP280_H


#include "core/pif_i2c.h"
#include "core/pif_task.h"


typedef enum EnPifBmp280Addr
{
	BMP280_I2C_ADDR_SDO_LOW		= 0x76,
	BMP280_I2C_ADDR_SDO_HIGH	= 0x77
} PifBmp280Addr;

typedef enum EnPifBmp280Reg
{
	BMP280_REG_CALIB			= 0x88,
	BMP280_REG_ID				= 0xD0,
	BMP280_REG_RESET			= 0xE0,
	BMP280_REG_STATUS			= 0xF3,
	BMP280_REG_CTRL_MEAS		= 0xF4,
	BMP280_REG_CCONFIG			= 0xF5,
	BMP280_REG_PRESS_MSB		= 0xF7,
	BMP280_REG_PRESS_LSB		= 0xF8,
	BMP280_REG_PRESS_XLSB		= 0xF9,
	BMP280_REG_TEMP_MSB			= 0xFA,
	BMP280_REG_TEMP_LSB			= 0xFB,
	BMP280_REG_TEMP_XLSB		= 0xFC
} PifBmp280Reg;


// Register : STATUS

#define BMP280_STATUS_IM_UPDATE		0x0001
#define BMP280_STATUS_MEASURING		0x0301

typedef union StPifBmp280Status
{
	uint8_t byte;
	struct {
		uint8_t im_update		: 1;	// LSB
		uint8_t reserved1		: 2;
		uint8_t measuring		: 1;
		uint8_t reserved2		: 4;	// MSB
	} bit;
} PifBmp280Status;


// Register : CTRL_MEAS

typedef enum EnPifBmp280Mode
{
	BMP280_MODE_SLEEP	= 0,
	BMP280_MODE_FORCED	= 1,
	BMP280_OSRS_NORMAL	= 3
} PifBmp280Mode;

typedef enum EnPifBmp280Osrs
{
	BMP280_OSRS_SKIP,
	BMP280_OSRS_X1,
	BMP280_OSRS_X2,
	BMP280_OSRS_X4,
	BMP280_OSRS_X8,
	BMP280_OSRS_X16,
} PifBmp280Osrs;

#define BMP280_CTRL_MEAS_MODE		0x0002
#define BMP280_CTRL_MEAS_OSRS_P		0x0203
#define BMP280_CTRL_MEAS_OSRS_T		0x0503

typedef union StPifBmp280CtrlMeas
{
	uint8_t byte;
	struct {
		uint8_t mode				: 2;	// LSB
		PifBmp280Osrs osrs_p		: 3;
		PifBmp280Osrs osrs_t		: 3;	// MSB
	} bit;
} PifBmp280CtrlMeas;


// Register : CONFIG

#define BMP280_CONFIG_SPI3W_EN		0x0001
#define BMP280_CONFIG_FILTER		0x0203
#define BMP280_CONFIG_T_SB			0x0503

typedef union StPifBmp280Config
{
	uint8_t byte;
	struct {
		uint8_t spi3w_en	: 1;	// LSB
		uint8_t reserved	: 1;
		uint8_t filter		: 3;
		uint8_t t_sb		: 3;	// MSB
	} bit;
} PifBmp280Config;


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


typedef void (*PifEvtBmp280Read)(int32_t pressure, float temperature);


/**
 * @class StPifBmp280
 * @brief
 */
typedef struct StPifBmp280
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;
	uint8_t _osrs_p;
	uint8_t _osrs_t;

	// Private Member Variable
	PifBmp280CalibParam __calib_param;
	PifTask* __p_task;
	uint16_t __read_period;
	PifBmp280State __state;

	// Private Event Function
	PifEvtBmp280Read __evt_read;
} PifBmp280;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmp280_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifBmp280_Init(PifBmp280* p_owner, PifId id, PifI2cPort* p_i2c, PifBmp280Addr addr);

/**
 * @fn pifBmp280_Clear
 * @brief
 * @param p_owner
 */
void pifBmp280_Clear(PifBmp280* p_owner);

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
BOOL pifBmp280_ReadBarometric(PifBmp280* p_owner, int32_t* p_pressure, float* p_temperature);

/**
 * @fn pifBmp280_AddTaskForReading
 * @brief
 * @param p_owner
 * @param read_period
 * @param evt_read
 * @return
 */
BOOL pifBmp280_AddTaskForReading(PifBmp280* p_owner, uint16_t read_period, PifEvtBmp280Read evt_read);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_H
