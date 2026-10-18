// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bmp280.h
 * @brief Bosch BMP280 barometric pressure and temperature sensor on I2C or SPI.
 *
 * pif_bmp280_i2c and pif_bmp280_spi attach the device. A task can trigger forced measurements and give the
 * pressure in hPa and the temperature in degrees C to evt_read.
 */
#ifndef PIF_BMP280_H
#define PIF_BMP280_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_sensor_event.h"


#define BMP280_I2C_ADDR(N)		(0x76 + (N))	///< I2C address with SDO on GND (0) or VDDIO (1)

#define BMP280_WHO_AM_I_CONST	0x58	///< Value of the ID register


/**
 * @brief Register addresses.
 */
typedef enum EnPifBmp280Reg
{
	BMP280_REG_CALIB			= 0x88,	///< First of the calibration registers, 0x88 to 0x9F
	BMP280_REG_ID				= 0xD0,	///< Chip ID
	BMP280_REG_RESET			= 0xE0,	///< Soft reset, written with 0xB6
	BMP280_REG_STATUS			= 0xF3,	///< Status
	BMP280_REG_CTRL_MEAS		= 0xF4,	///< Measurement control
	BMP280_REG_CONFIG			= 0xF5,	///< Rate, filter and interface
	BMP280_REG_PRESS_MSB		= 0xF7,	///< Pressure, bits 19-12
	BMP280_REG_PRESS_LSB		= 0xF8,	///< Pressure, bits 11-4
	BMP280_REG_PRESS_XLSB		= 0xF9,	///< Pressure, bits 3-0
	BMP280_REG_TEMP_MSB			= 0xFA,	///< Temperature, bits 19-12
	BMP280_REG_TEMP_LSB			= 0xFB,	///< Temperature, bits 11-4
	BMP280_REG_TEMP_XLSB		= 0xFC	///< Temperature, bits 3-0
} PifBmp280Reg;


// Register : STATUS

/** @name BMP280 STATUS register
 *  @{
 */
#define BMP280_IM_UPDATE(N)		(N)	///< Puts N into the IM_UPDATE field
#define BMP280_MEASURING(N)		((N) << 3)	///< Puts N into the MEASURING field

#define BMP280_IM_UPDATE_MASK	0x01	///< Mask of the IM_UPDATE field
#define BMP280_MEASURING_MASK	0x08	///< Mask of the MEASURING field
/** @} */


// Register : CTRL_MEAS

/** @name BMP280 CTRL_MEAS register
 *  @{
 */
/**
 * @brief Bits 1-0 (mode): power mode.
 */
typedef enum EnPifBmp280Mode
{
	BMP280_MODE_SLEEP		= 0,	///< No measurement
	BMP280_MODE_FORCED		= 1,	///< One measurement, then sleep
	BMP280_MODE_NORMAL		= 3	///< Measurements at the standby rate
} PifBmp280Mode;

/**
 * @brief Bits 4-2 (osrs_p): oversampling of the pressure.
 */
typedef enum EnPifBmp280OsrsP
{
	BMP280_OSRS_P_SKIP		= 0 << 2,
	BMP280_OSRS_P_X1		= 1 << 2,
	BMP280_OSRS_P_X2		= 2 << 2,
	BMP280_OSRS_P_X4		= 3 << 2,
	BMP280_OSRS_P_X8		= 4 << 2,
	BMP280_OSRS_P_X16		= 5 << 2
} PifBmp280OsrsP;

/**
 * @brief Bits 7-5 (osrs_t): oversampling of the temperature.
 */
typedef enum EnPifBmp280OsrsT
{
	BMP280_OSRS_T_SKIP		= 0 << 5,
	BMP280_OSRS_T_X1		= 1 << 5,
	BMP280_OSRS_T_X2		= 2 << 5,
	BMP280_OSRS_T_X4		= 3 << 5,
	BMP280_OSRS_T_X8		= 4 << 5,
	BMP280_OSRS_T_X16		= 5 << 5
} PifBmp280OsrsT;

#define BMP280_MODE_MASK	0x03	///< Mask of the MODE field
#define BMP280_OSRS_P_MASK	0x1C	///< Mask of the OSRS_P field
#define BMP280_OSRS_T_MASK	0xE0	///< Mask of the OSRS_T field
/** @} */


// Register : CONFIG

/** @name BMP280 CONFIG register
 *  @{
 */
#define BMP280_SPI3W_EN(N)		(N)	///< Puts N into the SPI3W_EN field

/**
 * @brief Bits 4-2 (filter): IIR filter coefficient.
 */
typedef enum EnPifBmp280Filter
{
	BMP280_FILTER_OFF			= 0 << 2,
	BMP280_FILTER_X2			= 1 << 2,
	BMP280_FILTER_X4			= 2 << 2,
	BMP280_FILTER_X8			= 3 << 2,
	BMP280_FILTER_X16			= 4 << 2
} PifBmp280Filter;

/**
 * @brief Bits 7-5 (t_sb): standby time between measurements in normal mode.
 */
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

#define BMP280_SPI3W_EN_MASK	0x01	///< Mask of the SPI3W_EN field
#define BMP280_FILTER_MASK		0x1C	///< Mask of the FILTER field
#define BMP280_T_SB_MASK		0xE0	///< Mask of the T_SB field
/** @} */


/**
 * @brief Step of the measurement task.
 */
typedef enum EnPifBmp280State
{
	BMP280_STATE_IDLE,	///< Not measuring
	BMP280_STATE_START,	///< A measurement is to be started
	BMP280_STATE_TRIGGER,	///< Forced mode is being set
	BMP280_STATE_WAIT,	///< Waiting for the measurement to finish
	BMP280_STATE_READ,	///< Reading the result
	BMP280_STATE_CALCURATE	///< Compensating the result
} PifBmp280State;


/// Trimming parameters read from registers 0x88 to 0x9F (datasheet table 17), and t_fine, the
/// intermediate temperature that the pressure compensation needs.
typedef struct StPifBmp280CalibParam 
{
    uint16_t dig_T1;				///< Trimming parameter dig_T1 of the datasheet
    int16_t dig_T2;				///< Trimming parameter dig_T2 of the datasheet
    int16_t dig_T3;				///< Trimming parameter dig_T3 of the datasheet
    uint16_t dig_P1;				///< Trimming parameter dig_P1 of the datasheet
    int16_t dig_P2;				///< Trimming parameter dig_P2 of the datasheet
    int16_t dig_P3;				///< Trimming parameter dig_P3 of the datasheet
    int16_t dig_P4;				///< Trimming parameter dig_P4 of the datasheet
    int16_t dig_P5;				///< Trimming parameter dig_P5 of the datasheet
    int16_t dig_P6;				///< Trimming parameter dig_P6 of the datasheet
    int16_t dig_P7;				///< Trimming parameter dig_P7 of the datasheet
    int16_t dig_P8;				///< Trimming parameter dig_P8 of the datasheet
    int16_t dig_P9;				///< Trimming parameter dig_P9 of the datasheet
    int32_t t_fine;				///< Fine temperature of the last compensation, used by the pressure compensation
} PifBmp280CalibParam;


/**
 * @class StPifBmp280
 * @brief Bosch BMP280 sensor.
 */
typedef struct StPifBmp280
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;	///< I2C or SPI device of the sensor
	uint8_t _osrs_p;	///< Pressure oversampling in use
	uint8_t _osrs_t;	///< Temperature oversampling in use
	PifTask* _p_task;	///< Measurement task, or NULL

	// Private Member Variable
	PifBmp280CalibParam __calib_param;
	uint16_t __read_period;
	uint16_t __delay;
	PifBmp280State __state;
	uint32_t __start_time;
	int32_t __raw_pressure;
	int32_t __raw_temperature;


	// Private Event Function
	PifEvtBaroRead __evt_read;
} PifBmp280;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads the calibration of the BMP280 and sets its filter and oversampling. Called by pifBmp280I2c_Init()
 *        and pifBmp280Spi_Init().
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280_Config(PifBmp280* p_owner, PifId id);

/**
 * @brief Sets the oversampling of the pressure and the temperature, and with it the measurement time.
 * @param p_owner Pointer to the owner instance.
 * @param osrs_p One of PifBmp280OsrsP.
 * @param osrs_t One of PifBmp280OsrsT.
 */
void pifBmp280_SetOverSamplingRate(PifBmp280* p_owner, uint8_t osrs_p, uint8_t osrs_t);

/**
 * @brief Asks the device for one measurement, in forced mode, and returns at once. It takes
 *        roughly as long as pifBmp280_ReadRawData() should be left before it is first asked,
 *        after which that call reports the result.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE if the measurement was started, otherwise FALSE.
 */
BOOL pifBmp280_StartMeasurement(PifBmp280* p_owner);

/**
 * @brief Reads the measurement that pifBmp280_StartMeasurement() asked for, waiting for nothing.
 *        FALSE means the sample is not there yet, so ask again from a later release of the task
 *        rather than looping here: the device is what says when the measurement is over, and
 *        holding the CPU until it does would delay every other task by the whole measurement.
 *        FALSE also covers a failed transfer, which means the same thing to a caller that is
 *        going to ask again.
 *        pifBmp280_AttachTaskForReading() does all of this and hands over compensated values, so
 *        prefer it unless the reading has to be driven by hand.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Receives the raw pressure.
 * @param p_temperature Receives the raw temperature.
 * @return TRUE when the sample was read, FALSE while it is not ready or on failure.
 */
BOOL pifBmp280_ReadRawData(PifBmp280* p_owner, int32_t* p_pressure, int32_t* p_temperature);

/**
 * @brief Compensates the measurement that pifBmp280_StartMeasurement() asked for. It waits for
 *        nothing and reports FALSE while the sample is not ready, exactly as
 *        pifBmp280_ReadRawData() does.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Receives the pressure in hPa.
 * @param p_temperature Receives the temperature in degrees C.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280_ReadBarometric(PifBmp280* p_owner, float* p_pressure, float* p_temperature);

/**
 * @brief Adds a task that triggers a forced measurement every read_period ms and gives the result to evt_read.
 * @param p_owner Pointer to the owner instance.
 * @param id Task ID, or PIF_ID_AUTO.
 * @param read_period Period of the readings in ms.
 * @param evt_read Gets each pressure in hPa and temperature in degrees C.
 * @param start Set to TRUE to start the task immediately.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280_AttachTaskForReading(PifBmp280* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_H
