// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_mpu30x0.h
 * @brief InvenSense MPU-3000 and MPU-3050 3-axis gyroscope on I2C, as the gyroscope of a PifImuSensor.
 *
 * The register map below follows the datasheet. The driver sets the range, the filter and the clock and reads
 * the gyro and the temperature.
 */
#ifndef PIF_MPU30X0_H
#define PIF_MPU30X0_H


#include "communication/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define MPU30X0_I2C_ADDR			0x68	///< I2C address with AD0 on GND. 0x69 with AD0 high.


/**
 * @brief Register addresses, named as in the datasheet.
 */
typedef enum EnPifMpu30x0Reg
{
	MPU30X0_REG_WHO_AM_I	     	= 0x00,
	MPU30X0_REG_PRODUCT_ID	     	= 0x01,
	MPU30X0_REG_X_OFFS_USRH      	= 0x0C,
	MPU30X0_REG_X_OFFS_USRL      	= 0x0D,
	MPU30X0_REG_Y_OFFS_USRH      	= 0x0E,
	MPU30X0_REG_Y_OFFS_USRL      	= 0x0F,
	MPU30X0_REG_Z_OFFS_USRH      	= 0x10,
	MPU30X0_REG_Z_OFFS_USRL      	= 0x11,
	MPU30X0_REG_FIFO_EN        	  	= 0x12,
	MPU30X0_REG_AUX_VDDIO      	  	= 0x13,
	MPU30X0_REG_AUX_SLV_ADDR   	  	= 0x14,
	MPU30X0_REG_SMPLRT_DIV      	= 0x15,
	MPU30X0_REG_DLPF_FS_SYNC      	= 0x16,
	MPU30X0_REG_INT_CFG        	  	= 0x17,
	MPU30X0_REG_AUX_ADDR   	  		= 0x18,
	MPU30X0_REG_INT_STATUS       	= 0x1A,
	MPU30X0_REG_TEMP_OUT_H      	= 0x1B,
	MPU30X0_REG_TEMP_OUT_L      	= 0x1C,
	MPU30X0_REG_GYRO_XOUT_H     	= 0x1D,
	MPU30X0_REG_GYRO_XOUT_L     	= 0x1E,
	MPU30X0_REG_GYRO_YOUT_H     	= 0x1F,
	MPU30X0_REG_GYRO_YOUT_L      	= 0x20,
	MPU30X0_REG_GYRO_ZOUT_H     	= 0x21,
	MPU30X0_REG_GYRO_ZOUT_L     	= 0x22,
	MPU30X0_REG_ACCEL_XOUT_H    	= 0x23,
	MPU30X0_REG_ACCEL_XOUT_L     	= 0x24,
	MPU30X0_REG_ACCEL_YOUT_H     	= 0x25,
	MPU30X0_REG_ACCEL_YOUT_L     	= 0x26,
	MPU30X0_REG_ACCEL_ZOUT_H    	= 0x27,
	MPU30X0_REG_ACCEL_ZOUT_L    	= 0x28,
	MPU30X0_REG_FIFO_COUNTH     	= 0x3A,
	MPU30X0_REG_FIFO_COUNTL     	= 0x3B,
	MPU30X0_REG_FIFO_R	        	= 0x3C,
	MPU30X0_REG_USER_CTRL       	= 0x3D,
	MPU30X0_REG_PWR_MGMT	      	= 0x3E
} PifMpu30x0Reg;


// Register : WHO_AM_I

/** @name MPU30X0 WHO_AM_I register
 *  @{
 */
#define MPU30X0_ID(N)				((N) << 1)	///< Puts N into the ID field
#define MPU30X0_I2C_IF_DIS(N)		((N) << 7)	///< Puts N into the I2C_IF_DIS field

#define MPU30X0_ID_MASK				0x7E	///< Mask of the ID field
#define MPU30X0_I2C_IF_DIS_MASK		0x80	///< Mask of the I2C_IF_DIS field
/** @} */


// Register : PRODUCT_ID

/** @name MPU30X0 PRODUCT_ID register
 *  @{
 */
#define MPU30X0_VERSION(N)		(N)	///< Puts N into the VERSION field
#define MPU30X0_PART_NUM(N)		((N) << 4)	///< Puts N into the PART_NUM field

#define MPU30X0_VERSION_MASK	0x0F	///< Mask of the VERSION field
#define MPU30X0_PART_NUM_MASK	0xF0	///< Mask of the PART_NUM field
/** @} */


// Register : FIFO_EN

/** @name MPU30X0 FIFO_EN register
 *  @{
 */
#define MPU30X0_FIFO_FOOTER(N)		(N)	///< Puts N into the FIFO_FOOTER field
#define MPU30X0_AUX_ZOUT(N)			((N) << 1)	///< Puts N into the AUX_ZOUT field
#define MPU30X0_AUX_YOUT(N)			((N) << 2)	///< Puts N into the AUX_YOUT field
#define MPU30X0_AUX_XOUT(N)			((N) << 3)	///< Puts N into the AUX_XOUT field
#define MPU30X0_GYRO_ZOUT(N)		((N) << 4)	///< Puts N into the GYRO_ZOUT field
#define MPU30X0_GYRO_YOUT(N)		((N) << 5)	///< Puts N into the GYRO_YOUT field
#define MPU30X0_GYRO_XOUT(N)		((N) << 6)	///< Puts N into the GYRO_XOUT field
#define MPU30X0_TEMP_OUT(N)			((N) << 7)	///< Puts N into the TEMP_OUT field

#define MPU30X0_FIFO_FOOTER_MASK	0x01	///< Mask of the FIFO_FOOTER field
#define MPU30X0_AUX_ZOUT_MASK		0x02	///< Mask of the AUX_ZOUT field
#define MPU30X0_AUX_YOUT_MASK		0x04	///< Mask of the AUX_YOUT field
#define MPU30X0_AUX_XOUT_MASK		0x08	///< Mask of the AUX_XOUT field
#define MPU30X0_GYRO_ZOUT_MASK		0x10	///< Mask of the GYRO_ZOUT field
#define MPU30X0_GYRO_YOUT_MASK		0x20	///< Mask of the GYRO_YOUT field
#define MPU30X0_GYRO_XOUT_MASK		0x40	///< Mask of the GYRO_XOUT field
#define MPU30X0_TEMP_OUT_MASK		0x80	///< Mask of the TEMP_OUT field
/** @} */


// Register : AUX_VDDIO

/** @name MPU30X0 AUX_VDDIO register
 *  @{
 */
#define MPU30X0_AUX_VDDIO(N)	((N) << 2)	///< Puts N into the AUX_VDDIO field

#define MPU30X0_AUX_VDDIO_MASK	0x04	///< Mask of the AUX_VDDIO field
/** @} */


// Register : AUX_SLV_ADDR

/** @name MPU30X0 AUX_SLV_ADDR register
 *  @{
 */
#define MPU30X0_AUX_ID(N)		(N)	///< Puts N into the AUX_ID field
#define MPU30X0_CLKOUT_EN(N)	((N) << 7)	///< Puts N into the CLKOUT_EN field

#define MPU30X0_AUX_ID_MASK		0x7F	///< Mask of the AUX_ID field
#define MPU30X0_CLKOUT_EN_MASK	0x80	///< Mask of the CLKOUT_EN field
/** @} */


// Register : DLPF_FS_SYNC

/** @name MPU30X0 DLPF_FS_SYNC register
 *  @{
 */
/**
 * @brief Bits 2-0 (DLPF_CFG): bandwidth of the digital low pass filter.
 */
typedef enum EnPifMpu30x0DlpfCfg
{
	MPU30X0_DLPF_CFG_256HZ				= 0,
	MPU30X0_DLPF_CFG_188HZ				= 1,
	MPU30X0_DLPF_CFG_98HZ				= 2,
	MPU30X0_DLPF_CFG_42HZ				= 3,
	MPU30X0_DLPF_CFG_20HZ				= 4,
	MPU30X0_DLPF_CFG_10HZ				= 5,
    MPU30X0_DLPF_CFG_5HZ				= 6
} PifMpu30x0DlpfCfg;

/**
 * @brief Bits 4-3 (FS_SEL): full scale range of the gyro.
 */
typedef enum EnPifMpu30x0FsSel
{
    MPU30X0_FS_SEL_250DPS				= 0 << 3,
    MPU30X0_FS_SEL_500DPS				= 1 << 3,
    MPU30X0_FS_SEL_1000DPS				= 2 << 3,
    MPU30X0_FS_SEL_2000DPS				= 3 << 3
} PifMpu30x0FsSel;

/**
 * @brief Bits 7-5 (EXT_SYNC_SET): register whose LSB carries the external sync input.
 */
typedef enum EnPifMpu30x0ExtSyncSet
{
	MPU30X0_EXT_SYNC_SET_NO_SYNC		= 0 << 5,
	MPU30X0_EXT_SYNC_SET_TEMP_OUT_L		= 1 << 5,
	MPU30X0_EXT_SYNC_SET_GYRO_XOUT_L	= 2 << 5,
	MPU30X0_EXT_SYNC_SET_GYRO_YOUT_L	= 3 << 5,
	MPU30X0_EXT_SYNC_SET_GYRO_ZOUT_L	= 4 << 5,
	MPU30X0_EXT_SYNC_SET_AUX_XOUT_L		= 5 << 5,
    MPU30X0_EXT_SYNC_SET_AUX_YOUT_L		= 6 << 5,
    MPU30X0_EXT_SYNC_SET_AUX_ZOUT_L		= 7 << 5
} PifMpu30x0ExtSyncSet;

#define MPU30X0_DLPF_CFG_MASK			0x07	///< Mask of the DLPF_CFG field
#define MPU30X0_FS_SEL_MASK				0x18	///< Mask of the FS_SEL field
#define MPU30X0_EXT_SYNC_SET_MASK		0xE0	///< Mask of the EXT_SYNC_SET field
/** @} */


// Register : INT_CFG

/** @name MPU30X0 INT_CFG register
 *  @{
 */
#define MPU30X0_RAW_RDY_EN(N)			(N)	///< Puts N into the RAW_RDY_EN field
#define MPU30X0_DMP_DONE_EN(N)			((N) << 1)	///< Puts N into the DMP_DONE_EN field
#define MPU30X0_MPU_RDY_EN(N)			((N) << 2)	///< Puts N into the MPU_RDY_EN field
#define MPU30X0_INT_ANYRD_2CLEAR(N)		((N) << 4)	///< Puts N into the INT_ANYRD_2CLEAR field
#define MPU30X0_LATCH_INT_EN(N)			((N) << 5)	///< Puts N into the LATCH_INT_EN field
#define MPU30X0_OPEN(N)					((N) << 6)	///< Puts N into the OPEN field
#define MPU30X0_ACTL(N)					((N) << 7)	///< Puts N into the ACTL field

#define MPU30X0_RAW_RDY_EN_MASK			0x01	///< Mask of the RAW_RDY_EN field
#define MPU30X0_DMP_DONE_EN_MASK		0x02	///< Mask of the DMP_DONE_EN field
#define MPU30X0_MPU_RDY_EN_MASK			0x04	///< Mask of the MPU_RDY_EN field
#define MPU30X0_INT_ANYRD_2CLEAR_MASK	0x10	///< Mask of the INT_ANYRD_2CLEAR field
#define MPU30X0_LATCH_INT_EN_MASK		0x20	///< Mask of the LATCH_INT_EN field
#define MPU30X0_OPEN_MASK				0x40	///< Mask of the OPEN field
#define MPU30X0_ACTL_MASK				0x80	///< Mask of the ACTL field
/** @} */


// Register : INT_STATUS

/** @name MPU30X0 INT_STATUS register
 *  @{
 */
#define MPU30X0_RAW_DATA_RDY(N)		(N)	///< Puts N into the RAW_DATA_RDY field
#define MPU30X0_DMP_DONE(N)			((N) << 1)	///< Puts N into the DMP_DONE field
#define MPU30X0_MPU_RDY(N)			((N) << 2)	///< Puts N into the MPU_RDY field

#define MPU30X0_RAW_DATA_RDY_MASK	0x01	///< Mask of the RAW_DATA_RDY field
#define MPU30X0_DMP_DONE_MASK		0x02	///< Mask of the DMP_DONE field
#define MPU30X0_MPU_RDY_MASK		0x04	///< Mask of the MPU_RDY field
/** @} */


// Register : USER_CTRL

/** @name MPU30X0 USER_CTRL register
 *  @{
 */
#define MPU30X0_GYRO_RST(N)			(N)	///< Puts N into the GYRO_RST field
#define MPU30X0_FIFO_RST(N)			((N) << 1)	///< Puts N into the FIFO_RST field
#define MPU30X0_AUX_IF_RST(N)		((N) << 3)	///< Puts N into the AUX_IF_RST field
#define MPU30X0_AUX_IF_EN(N)		((N) << 5)	///< Puts N into the AUX_IF_EN field
#define MPU30X0_FIFO_EN(N)			((N) << 6)	///< Puts N into the FIFO_EN field

#define MPU30X0_GYRO_RST_MASK		0x01	///< Mask of the GYRO_RST field
#define MPU30X0_FIFO_RST_MASK		0x02	///< Mask of the FIFO_RST field
#define MPU30X0_AUX_IF_RST_MASK		0x08	///< Mask of the AUX_IF_RST field
#define MPU30X0_AUX_IF_EN_MASK		0x20	///< Mask of the AUX_IF_EN field
#define MPU30X0_FIFO_EN_MASK		0x40	///< Mask of the FIFO_EN field
/** @} */


// Register : PWR_MGMT

/** @name MPU30X0 PWR_MGMT register
 *  @{
 */
/**
 * @brief Bits 2-0 (CLK_SEL): clock source.
 */
typedef enum EnPifMpu30x0ClkSel
{
    MPU30X0_CLK_SEL_INTERNAL_OSCILLATOR  	= 0,
    MPU30X0_CLK_SEL_PLL_XGYRO       		= 1,
    MPU30X0_CLK_SEL_PLL_YGYRO       		= 2,
    MPU30X0_CLK_SEL_PLL_ZGYRO       		= 3,
    MPU30X0_CLK_SEL_EXTERNAL_32KHZ  		= 4,
    MPU30X0_CLK_SEL_EXTERNAL_19MHZ  		= 5,
    MPU30X0_CLK_SEL_STOP_CLOCK      		= 7
} PifMpu30x0ClkSel;

#define MPU30X0_STBY_ZG(N)					((N) << 3)	///< Puts N into the STBY_ZG field
#define MPU30X0_STBY_YG(N)					((N) << 4)	///< Puts N into the STBY_YG field
#define MPU30X0_STBY_XG(N)					((N) << 5)	///< Puts N into the STBY_XG field
#define MPU30X0_SLEEP(N)					((N) << 6)	///< Puts N into the SLEEP field
#define MPU30X0_H_RESET(N)					((N) << 7)	///< Puts N into the H_RESET field

#define MPU30X0_CLK_SEL_MASK				0x07	///< Mask of the CLK_SEL field
#define MPU30X0_STBY_ZG_MASK				0x08	///< Mask of the STBY_ZG field
#define MPU30X0_STBY_YG_MASK				0x10	///< Mask of the STBY_YG field
#define MPU30X0_STBY_XG_MASK				0x20	///< Mask of the STBY_XG field
#define MPU30X0_SLEEP_MASK					0x40	///< Mask of the SLEEP field
#define MPU30X0_H_RESET_MASK				0x80	///< Mask of the H_RESET field
/** @} */


/**
 * @class StPifMpu30x0
 * @brief MPU-3000/3050 gyroscope.
 */
typedef struct StPifMpu30x0
{
	// Public Member Variable
	uint8_t scale;	///< Divider of the raw gyro values, 0 for none
	uint8_t temp_scale;	///< Multiplier of the temperature

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;	///< I2C device of the sensor

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifMpu30x0;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether an MPU-3000/3050 answers at addr, by reading its WHO_AM_I register through a temporary
 *        device.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Adds the MPU-3000/3050 at addr to the I2C port, wakes it up and registers it as the gyro of an IMU
 *        sensor.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_Init(PifMpu30x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifMpu30x0_Clear(PifMpu30x0* p_owner);

/**
 * @brief Writes the DLPF_FS_SYNC register and updates the gyro gain of the IMU sensor to its range.
 * @param p_owner Pointer to the owner instance.
 * @param dlpf_fs_sync Fields of the register, ORed together.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_SetDlpfFsSync(PifMpu30x0* p_owner, uint8_t dlpf_fs_sync);

/**
 * @brief Sets the gyro range and updates the gyro gain of the IMU sensor to it.
 * @param p_owner Pointer to the owner instance.
 * @param fs_sel Gyroscope full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_SetFsSel(PifMpu30x0* p_owner, PifMpu30x0FsSel fs_sel);

/**
 * @brief Reads the raw gyro values of the 3 axes, divided by scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_ReadGyro(PifMpu30x0* p_owner, int16_t* p_gyro);

/**
 * @brief Reads the temperature in degrees Celsius multiplied by temp_scale, as the MPU60x0 and
 *        MPU6500 drivers do.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Receives the temperature in degrees C, multiplied by temp_scale.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_ReadTemperature(PifMpu30x0* p_owner, int16_t* p_temperature);

/**
 * @brief Measures the gyro bias and noise at rest, taking a sample every 5 ms.
 *        Wraps pifImuSensor_CalibrateGyro().
 * @param p_owner Pointer to the owner instance.
 * @param samples Number of samples to average.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_CalibrationGyro(PifMpu30x0* p_owner, uint8_t samples);

/**
 * @brief Sets the gyro deadband as a multiple of the measured noise, calibrating
 *        with 50 samples first if needed. Wraps pifImuSensor_SetGyroDeadband().
 * @param p_owner Pointer to the owner instance.
 * @param multiple Deadband width in noise standard deviations. 0 turns it off.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_SetThreshold(PifMpu30x0* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU30X0_H
