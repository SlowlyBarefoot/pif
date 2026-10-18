// SPDX-License-Identifier: BSD-3-Clause AND MIT
/**
 * @file pif_dps310.h
 * @brief Infineon DPS310 barometric pressure and temperature sensor on I2C or SPI.
 *
 * pif_dps310_i2c and pif_dps310_spi attach the device. A task reads the measurements in the background mode and
 * gives the pressure in hPa and the temperature in degrees C to evt_read.
 */

#ifndef PIF_DPS310_H
#define PIF_DPS310_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_sensor_event.h"


#define DPS310_I2C_ADDR(N)		(0x76 + (N))	///< 0 ~ 1, default = 1

#define DPS310_PRODUCT_ID_CONST	0x10			///< Value of the PRODUCT_ID register


/**
 * @brief Register addresses.
 */
typedef enum EnPifDps310Reg
{
	DPS310_REG_PRS_B2			= 0x00,	///< Pressure, bits 23-16
	DPS310_REG_PRS_B1			= 0x01,	///< Pressure, bits 15-8
	DPS310_REG_PRS_B0			= 0x02,	///< Pressure, bits 7-0
	DPS310_REG_TMP_B2			= 0x03,	///< Temperature, bits 23-16
	DPS310_REG_TMP_B1			= 0x04,	///< Temperature, bits 15-8
	DPS310_REG_TMP_B0			= 0x05,	///< Temperature, bits 7-0
	DPS310_REG_PRS_CFG			= 0x06,	///< Pressure configuration
	DPS310_REG_TMP_CFG			= 0x07,	///< Temperature configuration
	DPS310_REG_MEAS_CFG			= 0x08,	///< Sensor operating mode and status
	DPS310_REG_CFG_REG			= 0x09,	///< Interrupt and FIFO configuration
	DPS310_REG_INT_STS			= 0x0A,	///< Interrupt status
	DPS310_REG_FIFO_STS			= 0x0B,	///< FIFO status
	DPS310_REG_RESET			= 0x0C,	///< Soft reset and FIFO flush
	DPS310_REG_PRODUCT_ID		= 0x0D,	///< Product and revision ID
	DPS310_REG_COEF				= 0x10,	///< First of the calibration coefficients, 0x10 to 0x21
	DPS310_REG_COEF_SPCE		= 0x28	///< Coefficient source of the temperature
} PifDps310Reg;


// Register : PRS_CFG

/** @name DPS310 PRS_CFG register
 *  @{
 */
/**
 * @brief Bits 3-0 (PM_PRC): pressure oversampling, in measurements per result.
 */
typedef enum EnPifDps310PmPrc
{
	DPS310_PM_PRC_SINGLE		= 0,
	DPS310_PM_PRC_2				= 1,
	DPS310_PM_PRC_4				= 2,
	DPS310_PM_PRC_8				= 3,
	DPS310_PM_PRC_16			= 4,
	DPS310_PM_PRC_32			= 5,
	DPS310_PM_PRC_64			= 6,
	DPS310_PM_PRC_128			= 7
} PifDps310PmPrc;

/**
 * @brief Bits 6-4 (PM_RATE): pressure measurements per second in background mode.
 */
typedef enum EnPifDps310PmRate
{
	DPS310_PM_RATE_1HZ			= 0 << 4,
	DPS310_PM_RATE_2HZ			= 1 << 4,
	DPS310_PM_RATE_4HZ			= 2 << 4,
	DPS310_PM_RATE_8HZ			= 3 << 4,
	DPS310_PM_RATE_16HZ			= 4 << 4,
	DPS310_PM_RATE_32HZ			= 5 << 4,
	DPS310_PM_RATE_64HZ			= 6 << 4,
	DPS310_PM_RATE_128HZ		= 7 << 4
} PifDps310PmRate;

#define DPS310_PM_PRC_MASK		0x0F	///< Mask of the PM_PRC field
#define DPS310_PM_RATE_MASK		0x70	///< Mask of the PM_RATE field
/** @} */


// Register : TMP_CFG

/** @name DPS310 TMP_CFG register
 *  @{
 */
/**
 * @brief Bits 3-0 (TMP_PRC): temperature oversampling, in measurements per result.
 */
typedef enum EnPifDps310TmpPrc
{
	DPS310_TMP_PRC_SINGLE		= 0,
	DPS310_TMP_PRC_2			= 1,
	DPS310_TMP_PRC_4			= 2,
	DPS310_TMP_PRC_8			= 3,
	DPS310_TMP_PRC_16			= 4,
	DPS310_TMP_PRC_32			= 5,
	DPS310_TMP_PRC_64			= 6,
	DPS310_TMP_PRC_128			= 7
} PifDps310TmpPrc;

/**
 * @brief Bits 6-4 (TMP_RATE): temperature measurements per second in background mode.
 */
typedef enum EnPifDps310TmpRate
{
	DPS310_TMP_RATE_1HZ			= 0 << 4,
	DPS310_TMP_RATE_2HZ			= 1 << 4,
	DPS310_TMP_RATE_4HZ			= 2 << 4,
	DPS310_TMP_RATE_8HZ			= 3 << 4,
	DPS310_TMP_RATE_16HZ		= 4 << 4,
	DPS310_TMP_RATE_32HZ		= 5 << 4,
	DPS310_TMP_RATE_64HZ		= 6 << 4,
	DPS310_TMP_RATE_128HZ		= 7 << 4
} PifDps310TmpRate;

#define DPS310_TMP_EXT(N)		((N) << 7)	///< Puts N into the TMP_EXT field

#define DPS310_TMP_PRC_MASK		0x0F	///< Mask of the TMP_PRC field
#define DPS310_TMP_RATE_MASK	0x70	///< Mask of the TMP_RATE field
#define DPS310_TMP_EXT_MASK		0x80	///< Mask of the TMP_EXT field
/** @} */


// Register : MEAS_CFG

/** @name DPS310 MEAS_CFG register
 *  @{
 */
/**
 * @brief Bits 2-0 (MEAS_CTRL): measurement mode.
 */
typedef enum EnPifDps310MeasCtrl
{
	DPS310_MEAS_CTRL_IDLE			= 0,	///< Idle, no measurement
	DPS310_MEAS_CTRL_PRS			= 1,	///< One pressure measurement
	DPS310_MEAS_CTRL_TMP			= 2,	///< One temperature measurement
	DPS310_MEAS_CTRL_CONT_PRS		= 5,	///< Continuous pressure measurement
	DPS310_MEAS_CTRL_CONT_TMP		= 6,	///< Continuous temperature measurement
	DPS310_MEAS_CTRL_CONT_PRS_TMP	= 7	///< Continuous pressure and temperature measurement
} PifDps310MeasCtrl;

#define DPS310_PRS_RDY(N)			((N) << 4)	///< Puts N into the PRS_RDY field
#define DPS310_TMP_RDY(N)			((N) << 5)	///< Puts N into the TMP_RDY field
#define DPS310_SENSOR_RDY(N)		((N) << 6)	///< Puts N into the SENSOR_RDY field
#define DPS310_COEF_RDY(N)			((N) << 7)	///< Puts N into the COEF_RDY field

#define DPS310_MEAS_CTRL_MASK		0x07	///< Mask of the MEAS_CTRL field
#define DPS310_PRS_RDY_MASK			0x10	///< Mask of the PRS_RDY field
#define DPS310_TMP_RDY_MASK			0x20	///< Mask of the TMP_RDY field
#define DPS310_SENSOR_RDY_MASK		0x40	///< Mask of the SENSOR_RDY field
#define DPS310_COEF_RDY_MASK		0x80	///< Mask of the COEF_RDY field
/** @} */


// Register : CFG_REG

/** @name DPS310 CFG_REG register
 *  @{
 */
#define DPS310_SPI_MODE(N)		(N)	///< Puts N into the SPI_MODE field
#define DPS310_FIFO_EN(N)		((N) << 1)	///< Puts N into the FIFO_EN field
#define DPS310_P_SHIFT(N)		((N) << 2)	///< Puts N into the P_SHIFT field
#define DPS310_T_SHIFT(N)		((N) << 3)	///< Puts N into the T_SHIFT field
#define DPS310_INT_PRS(N)		((N) << 4)	///< Puts N into the INT_PRS field
#define DPS310_INT_TMP(N)		((N) << 5)	///< Puts N into the INT_TMP field
#define DPS310_INT_FIFO(N)		((N) << 6)	///< Puts N into the INT_FIFO field
#define DPS310_INT_HL(N)		((N) << 7)	///< Puts N into the INT_HL field

#define DPS310_SPI_MODE_MASK	0x01	///< Mask of the SPI_MODE field
#define DPS310_FIFO_EN_MASK		0x02	///< Mask of the FIFO_EN field
#define DPS310_P_SHIFT_MASK		0x04	///< Mask of the P_SHIFT field
#define DPS310_T_SHIFT_MASK		0x08	///< Mask of the T_SHIFT field
#define DPS310_INT_PRS_MASK		0x10	///< Mask of the INT_PRS field
#define DPS310_INT_TMP_MASK		0x20	///< Mask of the INT_TMP field
#define DPS310_INT_FIFO_MASK	0x40	///< Mask of the INT_FIFO field
#define DPS310_INT_HL_MASK		0x80	///< Mask of the INT_HL field
/** @} */


// Register : INT_STS

/** @name DPS310 INT_STS register
 *  @{
 */
#define DPS310_INT_STS_PRS(N)		(N)	///< Puts N into the INT_STS_PRS field
#define DPS310_INT_STS_TMP(N)		((N) << 1)	///< Puts N into the INT_STS_TMP field
#define DPS310_INT_FIFO_FULL(N)		((N) << 2)	///< Puts N into the INT_FIFO_FULL field

#define DPS310_INT_STS_PRS_MASK		0x01	///< Mask of the INT_STS_PRS field
#define DPS310_INT_STS_TMP_MASK		0x02	///< Mask of the INT_STS_TMP field
#define DPS310_INT_FIFO_FULL_MASK	0x04	///< Mask of the INT_FIFO_FULL field
/** @} */


// Register : FIFO_STS

/** @name DPS310 FIFO_STS register
 *  @{
 */
#define DPS310_FIFO_EMPTY(N)	(N)	///< Puts N into the FIFO_EMPTY field
#define DPS310_FIFO_FULL(N)		((N) << 1)	///< Puts N into the FIFO_FULL field

#define DPS310_FIFO_EMPTY_MASK	0x01	///< Mask of the FIFO_EMPTY field
#define DPS310_FIFO_FULL_MASK	0x02	///< Mask of the FIFO_FULL field
/** @} */


// Register : RESET

/** @name DPS310 RESET register
 *  @{
 */
/**
 * @brief Bits 3-0 (SOFT_RST): value that resets the chip.
 */
typedef enum EnPifDps310SoftRst
{
	DPS310_RESET_SOFT_RST		= 9
} PifDps310SoftRst;

#define DPS310_FIFO_FLUSH(N)	((N) << 7)	///< Puts N into the FIFO_FLUSH field

#define DPS310_SOFT_RST_MASK	0x0F	///< Mask of the SOFT_RST field
#define DPS310_FIFO_FLUSH_MASK	0x80	///< Mask of the FIFO_FLUSH field
/** @} */


// Register : PRODUCT_ID

/** @name DPS310 PRODUCT_ID register
 *  @{
 */
#define DPS310_PROD_ID(N)		(N)	///< Puts N into the PROD_ID field
#define DPS310_REV_ID(N)		((N) << 4)	///< Puts N into the REV_ID field

#define DPS310_PROD_ID_MASK		0x0F	///< Mask of the PROD_ID field
#define DPS310_REV_ID_MASK		0xF0	///< Mask of the REV_ID field
/** @} */


// Register : COEF_SPCE

/** @name DPS310 COEF_SPCE register
 *  @{
 */
#define DPS310_TMP_COEF_SPCE(N)		((N) << 7)	///< Puts N into the TMP_COEF_SPCE field

#define DPS310_TMP_COEF_SPCE_MASK	0x80	///< Mask of the TMP_COEF_SPCE field
/** @} */


/**
 * @brief Step of the measurement task.
 */
typedef enum EnPifDps310State
{
	DPS310_STATE_IDLE,	///< Not measuring
	DPS310_STATE_READY,	///< Waiting for a measurement to be ready
	DPS310_STATE_READ,	///< Reading the result
	DPS310_STATE_CALCURATE	///< Compensating the result
} PifDps310State;


/**
 * @class StPifDps310
 * @brief Infineon DPS310 sensor.
 */
typedef struct StPifDps310
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;	///< I2C or SPI device of the sensor
	uint8_t _osrs_p;	///< Pressure oversampling in use
	uint8_t _osrs_t;	///< Temperature oversampling in use
	PifTask* _p_task;	///< Measurement task, or NULL

	// Private Member Variable
	uint16_t __read_period;
	PifDps310State __state;
	uint32_t __start_time;
	int32_t __raw_pressure;
	int32_t __raw_temperature;
	float __last_temp;
	/// compensation coefficients
	float __c0_half;
	int32_t __c1;
	int32_t __c00;
	int32_t __c10;
	int32_t __c01;
	int32_t __c11;
	int32_t __c20;
	int32_t __c21;
	int32_t __c30;


	// Private Event Function
	PifEvtBaroRead __evt_read;
} PifDps310;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks the product ID, resets the DPS310 and reads its calibration coefficients. It holds the CPU for
 *        40 ms. Called by pifDps310I2c_Init() and pifDps310Spi_Init().
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_Config(PifDps310* p_owner, PifId id);

/**
 * @brief Sets the oversampling and the background rate of the pressure.
 * @param p_owner Pointer to the owner instance.
 * @param osrs One of PifDps310PmPrc.
 * @param rate One of PifDps310PmRate.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifDps310_SetPressureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate);

/**
 * @brief Sets the oversampling and the background rate of the temperature.
 * @param p_owner Pointer to the owner instance.
 * @param osrs One of PifDps310TmpPrc.
 * @param rate One of PifDps310TmpRate.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifDps310_SetTemperatureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate);

/**
 * @brief Sets the measurement mode of the DPS310.
 * @param p_owner Pointer to the owner instance.
 * @param meas_ctrl One of PifDps310MeasCtrl.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_SensorOperatingMode(PifDps310* p_owner, uint8_t meas_ctrl);

/**
 * @brief Reads the raw pressure and temperature results.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Receives the raw pressure.
 * @param p_temperature Receives the raw temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_ReadRawData(PifDps310* p_owner, int32_t* p_pressure, int32_t* p_temperature);

/**
 * @brief Reads the results and compensates them with the calibration coefficients.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Receives the pressure in hPa.
 * @param p_temperature Receives the temperature in degrees C.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_ReadBarometric(PifDps310* p_owner, float* p_pressure, float* p_temperature);

/**
 * @brief Adds a task that reads the measurements every read_period ms and gives them to evt_read.
 * @param p_owner Pointer to the owner instance.
 * @param id Task ID, or PIF_ID_AUTO.
 * @param read_period Period of the readings in ms.
 * @param evt_read Gets each pressure in hPa and temperature in degrees C.
 * @param start Set to TRUE to start the task immediately.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_AttachTaskForReading(PifDps310* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_H
