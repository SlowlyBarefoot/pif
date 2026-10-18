// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bmi270.h
 * @brief Bosch BMI270 6-axis gyroscope and accelerometer, as a PifImuSensor.
 *
 * pif_bmi270_i2c and pif_bmi270_spi attach the device. Initialization loads the configuration file of the
 * feature engine. The register map below follows the datasheet.
 */
#ifndef PIF_BMI270_H
#define PIF_BMI270_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "sensor/pif_imu_sensor.h"


#define BMI270_I2C_ADDR(N)			(0x68 + (N))	///< I2C address with SDO on GND (0) or VDDIO (1)

#define BMI270_WHO_AM_I_CONST		0x24	///< Value of the CHIP_ID register


/**
 * @brief Register addresses, named as in the datasheet.
 */
typedef enum EnPifBmi270Reg
{
	BMI270_REG_CHIP_ID  			= 0x00,
	BMI270_REG_ERR_REG  			= 0x02,
	BMI270_REG_STATUS  				= 0x03,
	BMI270_REG_AUX_X_LSB  			= 0x04,		///< 6Bytes
	BMI270_REG_AUX_R_LSB  			= 0x0A,		///< 2Bytes
	BMI270_REG_ACC_X_LSB  			= 0x0C,		///< 6Bytes
	BMI270_REG_GYR_X_LSB  			= 0x12,		///< 6Bytes
	BMI270_REG_SENSOR_TIME_LSB      = 0x18,		///< 3Bytes
	BMI270_REG_EVENT      			= 0x1B,
	BMI270_REG_INT_STATUS_0    		= 0x1C,
	BMI270_REG_INT_STATUS_1    		= 0x1D,
	BMI270_REG_SC_OUT_LSB	    	= 0x1E,		///< 2Bytes
	BMI270_REG_WR_GEST_ACT	    	= 0x20,
	BMI270_REG_INTERNAL_STATUS   	= 0x21,
	BMI270_REG_TEMPERATURE_LSB    	= 0x22,		///< 2Bytes
	BMI270_REG_FIFO_LENGTH_LSB   	= 0x24,		///< 2Bytes
	BMI270_REG_FIFO_DATA	   	  	= 0x26,
	BMI270_REG_FEAT_PAGE	    	= 0x2F,

	BMI270_REG_ACC_CONF		    	= 0x40,
	BMI270_REG_ACC_RANGE	      	= 0x41,
	BMI270_REG_GYR_CONF		      	= 0x42,
	BMI270_REG_GYR_RANGE	     	= 0x43,
	BMI270_REG_AUX_CONF		     	= 0x44,
	BMI270_REG_FIFO_DOWNS	     	= 0x45,
	BMI270_REG_FIFO_WTM_0         	= 0x46,
	BMI270_REG_FIFO_WTM_1          	= 0x47,
	BMI270_REG_FIFO_CONFIG_0     	= 0x48,
	BMI270_REG_FIFO_CONFIG_1		= 0x49,
	BMI270_REG_SATURATION			= 0x4A,
	BMI270_REG_AUX_DEV_ID			= 0x4B,
	BMI270_REG_AUX_IF_CONF			= 0x4C,
	BMI270_REG_AUX_RD_ADDR			= 0x4D,
	BMI270_REG_AUX_WR_ADDR			= 0x4E,
	BMI270_REG_AUX_WR_DATA			= 0x4F,
	BMI270_REG_ERR_REG_MSK			= 0x52,
	BMI270_REG_INT1_IO_CTRL			= 0x53,
	BMI270_REG_INT2_IO_CTRL			= 0x54,
	BMI270_REG_INT_LATCH			= 0x55,
	BMI270_REG_INT1_MAP_FEAT		= 0x56,
	BMI270_REG_INT2_MAP_FEAT		= 0x57,
	BMI270_REG_INT_MAP_DATA			= 0x58,
	BMI270_REG_INIT_CTRL			= 0x59,
	BMI270_REG_INIT_ADDR_LSB		= 0x5B,
	BMI270_REG_INIT_ADDR_HSB		= 0x5C,
	BMI270_REG_INIT_DATA			= 0x5E,
	BMI270_REG_INTERNAL_ERROR		= 0x5F,
	BMI270_REG_AUX_IF_TRIM		  	= 0x68,
	BMI270_REG_GYR_CRT_CONF		  	= 0x69,
	BMI270_REG_NVM_CONF		       	= 0x6A,
	BMI270_REG_IF_CONF		      	= 0x6B,
	BMI270_REG_DRV			      	= 0x6C,
	BMI270_REG_ACC_SELF_TEST     	= 0x6D,
	BMI270_REG_GYR_SELF_TEST_AXES  	= 0x6E,
	BMI270_REG_NV_CONF		     	= 0x70,
	BMI270_REG_OFFSET_ACC_X     	= 0x71,		///< 3Byte
	BMI270_REG_OFFSET_GYR_USR_X    	= 0x74,		///< 3Byte
	BMI270_REG_OFFSET_6	        	= 0x77,
	BMI270_REG_PWR_CONF	        	= 0x7C,
	BMI270_REG_PWR_CTRL	        	= 0x7D,
	BMI270_REG_CMD		        	= 0x7E
} PifBmi270Reg;


// Register : ERR_REG(0x02)

/** @name BMI270 ERR_REG(0x02) register
 *  @{
 */
#define BMI270_ER_FATAL_ERR_MASK		0x01	///< Mask of the ER_FATAL_ERR field
#define BMI270_ER_INTERNAL_ERR_MASK		0x1E	///< Mask of the ER_INTERNAL_ERR field
#define BMI270_ER_FIFO_ERR_MASK			0x40	///< Mask of the ER_FIFO_ERR field
#define BMI270_ER_AUX_ERR_MASK			0x80	///< Mask of the ER_AUX_ERR field
/** @} */


// Register : STATUS(0x03)

/** @name BMI270 STATUS(0x03) register
 *  @{
 */
#define BMI270_S_AUX_BUSY_MASK		0x04	///< Mask of the S_AUX_BUSY field
#define BMI270_S_CMD_RDY_MASK		0x10	///< Mask of the S_CMD_RDY field
#define BMI270_S_DRDY_AUX_MASK		0x20	///< Mask of the S_DRDY_AUX field
#define BMI270_S_DRDY_GYR_MASK		0x40	///< Mask of the S_DRDY_GYR field
#define BMI270_S_DRDY_ACC_MASK		0x80	///< Mask of the S_DRDY_ACC field
/** @} */


// Register : EVENT(0x1B)

/** @name BMI270 EVENT(0x1B) register
 *  @{
 */
/**
 * @brief Bits 4-2 (error_code) of EVENT: error of the configuration load.
 */
typedef enum EnPifBmi270EErrorCode
{
    BMI270_E_ERROR_CODE_NO			= 0 << 2,
    BMI270_E_ERROR_CODE_ACC			= 1 << 2,
    BMI270_E_ERROR_CODE_GYR			= 2 << 2,
    BMI270_E_ERROR_CODE_ACC_GYR		= 3 << 2
} PifBmi270EErrorCode;

#define BMI270_E_POR_DETECTED_MASK	0x01	///< Mask of the E_POR_DETECTED field
#define BMI270_E_ERROR_CODE_MASK	0x1C	///< Mask of the E_ERROR_CODE field
/** @} */


// Register : INT_STATUS_0(0x1C)

/** @name BMI270 INT_STATUS_0(0x1C) register
 *  @{
 */
#define BMI270_IS0_SIG_MOTION_OUT_MASK			0x01	///< Mask of the IS0_SIG_MOTION_OUT field
#define BMI270_IS0_STEP_COUNTER_OUT_MASK		0x02	///< Mask of the IS0_STEP_COUNTER_OUT field
#define BMI270_IS0_ACTIVITY_OUT_MASK			0x04	///< Mask of the IS0_ACTIVITY_OUT field
#define BMI270_IS0_WRIST_WEAR_WAKEUP_OUT_MASK	0x08	///< Mask of the IS0_WRIST_WEAR_WAKEUP_OUT field
#define BMI270_IS0_WRIST_GESTURE_OUT_MASK		0x10	///< Mask of the IS0_WRIST_GESTURE_OUT field
#define BMI270_IS0_NO_MOTION_OUT_MASK			0x20	///< Mask of the IS0_NO_MOTION_OUT field
#define BMI270_IS0_ANY_MOTION_OUT_MASK			0x40	///< Mask of the IS0_ANY_MOTION_OUT field
/** @} */


// Register : INT_STATUS_1(0x1D)

/** @name BMI270 INT_STATUS_1(0x1D) register
 *  @{
 */
#define BMI270_IS1_FFULL_INT_MASK		0x01	///< Mask of the IS1_FFULL_INT field
#define BMI270_IS1_FWM_INT_MASK			0x02	///< Mask of the IS1_FWM_INT field
#define BMI270_IS1_ERR_INT_MASK			0x04	///< Mask of the IS1_ERR_INT field
#define BMI270_IS1_AUX_DRDY_INT_MASK	0x20	///< Mask of the IS1_AUX_DRDY_INT field
#define BMI270_IS1_GYR_DRDY_INT_MASK	0x40	///< Mask of the IS1_GYR_DRDY_INT field
#define BMI270_IS1_ACC_DRDY_INT_MASK	0x80	///< Mask of the IS1_ACC_DRDY_INT field
/** @} */


// Register : WR_GEST_ACT(0x20)

/** @name BMI270 WR_GEST_ACT(0x20) register
 *  @{
 */
/**
 * @brief Bits 2-0 (wr_gest_out): wrist gesture detected last.
 */
typedef enum EnPifBmi270WgaWrGestOut
{
    BMI270_WGA_WR_GEST_OUT_UNKNOWN				= 0,
    BMI270_WGA_WR_GEST_OUT_PUSH_ARM_DOWN		= 1,
    BMI270_WGA_WR_GEST_OUT_PIVOT_UP				= 2,
    BMI270_WGA_WR_GEST_OUT_WRIST_SHAKE_JIGGLE	= 3,
    BMI270_WGA_WR_GEST_OUT_FLICK_IN				= 4,
    BMI270_WGA_WR_GEST_OUT_FLICK_OUT			= 5
} PifBmi270WgaWrGestOut;

/**
 * @brief Bits 4-3 (act_out): activity detected last.
 */
typedef enum EnPifBmi270WgaActOut
{
    BMI270_WGA_ACT_OUT_STILL					= 0 << 3,
    BMI270_WGA_ACT_OUT_WALKING					= 1 << 3,
    BMI270_WGA_ACT_OUT_RUNNING					= 2 << 3,
    BMI270_WGA_ACT_OUT_UNKNOWN					= 3 << 3
} PifBmi270WgaActOut;

#define BMI270_WGA_WR_GEST_OUT_MASK				0x07	///< Mask of the WGA_WR_GEST_OUT field
#define BMI270_WGA_ACT_OUT_MASK					0x18	///< Mask of the WGA_ACT_OUT field
/** @} */


// Register : INTERNAL_STATUS(0x21)

/** @name BMI270 INTERNAL_STATUS(0x21) register
 *  @{
 */
/**
 * @brief Bits 3-0 (message): initialization status of the feature engine.
 */
typedef enum EnPifBmi270IsMessage
{
    BMI270_IS_MESSAGE_NOT_INIT			= 0,
    BMI270_IS_MESSAGE_INIT_OK			= 1,
    BMI270_IS_MESSAGE_INIT_ERR			= 2,
    BMI270_IS_MESSAGE_DRV_ERR			= 3,
    BMI270_IS_MESSAGE_SNS_STOP			= 4,
    BMI270_IS_MESSAGE_NVM_ERR			= 5,
    BMI270_IS_MESSAGE_START_UP_ERR		= 6,
    BMI270_IS_MESSAGE_COMPAT_ERR		= 7
} PifBmi270IsMessage;

#define BMI270_IS_MESSAGE_MASK			0x0F	///< Mask of the IS_MESSAGE field
#define BMI270_IS_AXES_REMAP_ERR_MASK	0x20	///< Mask of the IS_AXES_REMAP_ERR field
#define BMI270_IS_ODR_50HZ_ERR_MASK		0x40	///< Mask of the IS_ODR_50HZ_ERR field
/** @} */


// Register : ACC_CONF(0x40)

/** @name BMI270 ACC_CONF(0x40) register
 *  @{
 */
/**
 * @brief Bits 3-0 (acc_odr): output data rate of the accelerometer in Hz.
 */
typedef enum EnPifBmi270AcAccOdr
{
    BMI270_AC_ACC_ODR_RESERVED				= 0,
    BMI270_AC_ACC_ODR_0P78					= 1,
    BMI270_AC_ACC_ODR_1P5					= 2,
    BMI270_AC_ACC_ODR_3P1					= 3,
    BMI270_AC_ACC_ODR_6P25					= 4,
    BMI270_AC_ACC_ODR_12P5					= 5,
    BMI270_AC_ACC_ODR_25					= 6,
    BMI270_AC_ACC_ODR_50					= 7,
    BMI270_AC_ACC_ODR_100					= 8,
    BMI270_AC_ACC_ODR_200					= 9,
    BMI270_AC_ACC_ODR_400					= 10,
    BMI270_AC_ACC_ODR_800					= 11,
    BMI270_AC_ACC_ODR_1K6					= 12,
    BMI270_AC_ACC_ODR_3K2					= 13,
    BMI270_AC_ACC_ODR_6K4					= 14,
    BMI270_AC_ACC_ODR_12K8					= 15
} PifBmi270AcAccOdr;

/**
 * @brief Bits 6-4 (acc_bwp): bandwidth, as oversampling in normal mode or averaging in low power mode.
 */
typedef enum EnPifBmi270AcAccBwp
{
    BMI270_AC_ACC_BWP_OSR4_AVG1				= 0 << 4,
    BMI270_AC_ACC_BWP_OSR2_AVG1				= 1 << 4,
    BMI270_AC_ACC_BWP_NORM_AVG4				= 2 << 4,
    BMI270_AC_ACC_BWP_CIC_AVG8				= 3 << 4,
    BMI270_AC_ACC_BWP_RES_AVG16				= 4 << 4,
    BMI270_AC_ACC_BWP_RES_AVG32				= 5 << 4,
    BMI270_AC_ACC_BWP_RES_AVG64				= 6 << 4,
    BMI270_AC_ACC_BWP_RES_AVG128			= 7 << 4
} PifBmi270AcAccBwp;

/**
 * @brief Bit 7 (acc_filter_perf): power optimized or performance optimized filter.
 */
typedef enum EnPifBmi270AcAccFilterPerf
{
    BMI270_AC_ACC_FILTER_PERF_ULP			= 0 << 7,
    BMI270_AC_ACC_FILTER_PERF_HP			= 1 << 7
} PifBmi270AcAccFilterPerf;

#define BMI270_AC_ACC_ODR_MASK				0x0F	///< Mask of the AC_ACC_ODR field
#define BMI270_AC_ACC_BWP_MASK				0x70	///< Mask of the AC_ACC_BWP field
#define BMI270_AC_ACC_FILTER_PERF_MASK		0x80	///< Mask of the AC_ACC_FILTER_PERF field
/** @} */


// Register : ACC_RANGE(0x41)

/** @name BMI270 ACC_RANGE(0x41) register
 *  @{
 */
/**
 * @brief Bits 1-0 (acc_range): full scale range of the accelerometer.
 */
typedef enum EnPifBmi270ArAccRange
{
    BMI270_AR_ACC_RANGE_2G			= 0,
    BMI270_AR_ACC_RANGE_4G			= 1,
    BMI270_AR_ACC_RANGE_8G			= 2,
    BMI270_AR_ACC_RANGE_16G			= 3
} PifBmi270ArAccRange;

#define BMI270_AR_ACC_RANGE_MASK	0x03	///< Mask of the AR_ACC_RANGE field
/** @} */


// Register : GYR_CONF(0x42)

/** @name BMI270 GYR_CONF(0x42) register
 *  @{
 */
/**
 * @brief Bits 3-0 (gyr_odr): output data rate of the gyro in Hz.
 */
typedef enum EnPifBmi270GcGyrOdr
{
    BMI270_GC_GYR_ODR_RESERVED				= 0,
    BMI270_GC_GYR_ODR_0P78					= 1,
    BMI270_GC_GYR_ODR_1P5					= 2,
    BMI270_GC_GYR_ODR_3P1					= 3,
    BMI270_GC_GYR_ODR_6P25					= 4,
    BMI270_GC_GYR_ODR_12P5					= 5,
    BMI270_GC_GYR_ODR_25					= 6,
    BMI270_GC_GYR_ODR_50					= 7,
    BMI270_GC_GYR_ODR_100					= 8,
    BMI270_GC_GYR_ODR_200					= 9,
    BMI270_GC_GYR_ODR_400					= 10,
    BMI270_GC_GYR_ODR_800					= 11,
    BMI270_GC_GYR_ODR_1K6					= 12,
    BMI270_GC_GYR_ODR_3K2					= 13,
    BMI270_GC_GYR_ODR_6K4					= 14,
    BMI270_GC_GYR_ODR_12K8					= 15
} PifBmi270GcGyrOdr;

/**
 * @brief Bits 5-4 (gyr_bwp): bandwidth of the gyro filter.
 */
typedef enum EnPifBmi270GcGyrBwp
{
    BMI270_GC_GYR_BWP_OSR4					= 0 << 4,
    BMI270_GC_GYR_BWP_OSR2					= 1 << 4,
    BMI270_GC_GYR_BWP_NORM					= 2 << 4,
    BMI270_GC_GYR_BWP_RES					= 3 << 4
} PifBmi270GcGyrBwp;

/**
 * @brief Bit 6 (gyr_noise_perf): power or noise optimized gyro.
 */
typedef enum EnPifBmi270GcGyrNoisePerf
{
    BMI270_GC_GYR_NOISE_PERF_ULP			= 0 << 6,
    BMI270_GC_GYR_NOISE_PERF_HP				= 1 << 6
} PifBmi270GcGyrNoisePerf;

/**
 * @brief Bit 7 (gyr_filter_perf): power or performance optimized gyro filter.
 */
typedef enum EnPifBmi270GcGyrFilterPerf
{
    BMI270_GC_GYR_FILTER_PERF_ULP			= 0 << 7,
    BMI270_GC_GYR_FILTER_PERF_HP			= 1 << 7
} PifBmi270GcGyrFilterPerf;

#define BMI270_GC_GYR_ODR_MASK				0x0F	///< Mask of the GC_GYR_ODR field
#define BMI270_GC_GYR_BWP_MASK				0x30	///< Mask of the GC_GYR_BWP field
#define BMI270_GC_GYR_NOISE_PERF_MASK		0x40	///< Mask of the GC_GYR_NOISE_PERF field
#define BMI270_GC_GYR_FILTER_PERF_MASK		0x80	///< Mask of the GC_GYR_FILTER_PERF field
/** @} */


// Register : GYR_RANGE(0x43)

/** @name BMI270 GYR_RANGE(0x43) register
 *  @{
 */
/**
 * @brief Bits 2-0 (gyr_range): full scale range of the gyro in degrees per second.
 */
typedef enum EnPifBmi270GrGyrRange
{
    BMI270_GR_GYR_RANGE_2000		= 0,
    BMI270_GR_GYR_RANGE_1000		= 1,
    BMI270_GR_GYR_RANGE_500			= 2,
    BMI270_GR_GYR_RANGE_250			= 3,
    BMI270_GR_GYR_RANGE_125			= 4
} PifBmi270GrGyrRange;

/**
 * @brief Bit 3 (ois_range): full scale range of the OIS data.
 */
typedef enum EnPifBmi270GrOisRange
{
    BMI270_GR_OIS_RANGE_250			= 0 << 3,
    BMI270_GR_OIS_RANGE_2000		= 1 << 3
} PifBmi270GrOisRange;

#define BMI270_GR_GYR_RANGE_MASK	0x07	///< Mask of the GR_GYR_RANGE field
#define BMI270_GR_OIS_RANGE_MASK	0x08	///< Mask of the GR_OIS_RANGE field
/** @} */


// Register : AUX_CONF(0x44)

/** @name BMI270 AUX_CONF(0x44) register
 *  @{
 */
/**
 * @brief Bits 3-0 (aux_odr): output data rate of the auxiliary interface in Hz.
 */
typedef enum EnPifBmi270AcAuxOdr
{
    BMI270_AC_AUX_ODR_RESERVED		= 0,
    BMI270_AC_AUX_ODR_0P78			= 1,
    BMI270_AC_AUX_ODR_1P5			= 2,
    BMI270_AC_AUX_ODR_3P1			= 3,
    BMI270_AC_AUX_ODR_6P25			= 4,
    BMI270_AC_AUX_ODR_12P5			= 5,
    BMI270_AC_AUX_ODR_25			= 6,
    BMI270_AC_AUX_ODR_50			= 7,
    BMI270_AC_AUX_ODR_100			= 8,
    BMI270_AC_AUX_ODR_200			= 9,
    BMI270_AC_AUX_ODR_400			= 10,
    BMI270_AC_AUX_ODR_800			= 11,
    BMI270_AC_AUX_ODR_1K6			= 12,
    BMI270_AC_AUX_ODR_3K2			= 13,
    BMI270_AC_AUX_ODR_6K4			= 14,
    BMI270_AC_AUX_ODR_12K8			= 15
} PifBmi270AcAuxOdr;

#define BMI270_AC_AUX_OFFSET(N)		((N) << 4)	///< Puts N into the AC_AUX_OFFSET field

#define BMI270_AC_AUX_ODR_MASK		0x0F	///< Mask of the AC_AUX_ODR field
#define BMI270_AC_AUX_OFFSET_MASK	0xF0	///< Mask of the AC_AUX_OFFSET field
/** @} */


// Register : FIFO_DOWNS(0x45)

/** @name BMI270 FIFO_DOWNS(0x45) register
 *  @{
 */
#define BMI270_FD_GYR_FIFO_DOWNS(N)			(N)	///< Puts N into the FD_GYR_FIFO_DOWNS field
#define BMI270_FD_GYR_FIFO_FILT_DATA(N)		((N) << 3)	///< Puts N into the FD_GYR_FIFO_FILT_DATA field
#define BMI270_FD_ACC_FIFO_DOWNS(N)			((N) << 4)	///< Puts N into the FD_ACC_FIFO_DOWNS field
#define BMI270_FD_ACC_FIFO_FILT_DATA(N)		((N) << 7)	///< Puts N into the FD_ACC_FIFO_FILT_DATA field

#define BMI270_FD_GYR_FIFO_DOWNS_MASK		0x07	///< Mask of the FD_GYR_FIFO_DOWNS field
#define BMI270_FD_GYR_FIFO_FILT_DATA_MASK	0x08	///< Mask of the FD_GYR_FIFO_FILT_DATA field
#define BMI270_FD_ACC_FIFO_DOWNS_MASK		0x70	///< Mask of the FD_ACC_FIFO_DOWNS field
#define BMI270_FD_ACC_FIFO_FILT_DATA_MASK	0x80	///< Mask of the FD_ACC_FIFO_FILT_DATA field
/** @} */


// Register : FIFO_CONFIG_0(0x48)

/** @name BMI270 FIFO_CONFIG_0(0x48) register
 *  @{
 */
#define BMI270_FC0_FIFO_STOP_ON_FULL(N)		(N)	///< Puts N into the FC0_FIFO_STOP_ON_FULL field
#define BMI270_FC0_FIFO_TIME_EN(N)			((N) << 1)	///< Puts N into the FC0_FIFO_TIME_EN field

#define BMI270_FC0_FIFO_STOP_ON_FULL_MASK	0x01	///< Mask of the FC0_FIFO_STOP_ON_FULL field
#define BMI270_FC0_FIFO_TIME_EN_MASK		0x02	///< Mask of the FC0_FIFO_TIME_EN field
/** @} */


// Register : FIFO_CONFIG_1(0x49)

/** @name BMI270 FIFO_CONFIG_1(0x49) register
 *  @{
 */
/**
 * @brief Bits 1-0 (fifo_tag_int1_en): what the FIFO tags of INT1 record.
 */
typedef enum EnPifBmi270Fc1FifoTagInt1En
{
    BMI270_FC1_FIFO_TAG_INT1_EN_INT_EDGE	= 0,
    BMI270_FC1_FIFO_TAG_INT1_EN_INT_LEVEL	= 1,
    BMI270_FC1_FIFO_TAG_INT1_EN_ACC_SAT		= 2,
    BMI270_FC1_FIFO_TAG_INT1_EN_GYR_SAT		= 3
} PifBmi270Fc1FifoTagInt1En;

/**
 * @brief Bits 3-2 (fifo_tag_int2_en): what the FIFO tags of INT2 record.
 */
typedef enum EnPifBmi270Fc1FifoTagInt2En
{
    BMI270_FC1_FIFO_TAG_INT2_EN_INT_EDGE	= 0 << 2,
    BMI270_FC1_FIFO_TAG_INT2_EN_INT_LEVEL	= 1 << 2,
    BMI270_FC1_FIFO_TAG_INT2_EN_ACC_SAT		= 2 << 2,
    BMI270_FC1_FIFO_TAG_INT2_EN_GYR_SAT		= 3 << 2
} PifBmi270Fc1FifoTagInt2En;

#define BMI270_FC1_FIFO_HEADER_EN(N)		((N) << 4)	///< Puts N into the FC1_FIFO_HEADER_EN field
#define BMI270_FC1_FIFO_AUX_EN(N)			((N) << 5)	///< Puts N into the FC1_FIFO_AUX_EN field
#define BMI270_FC1_FIFO_ACC_EN(N)			((N) << 6)	///< Puts N into the FC1_FIFO_ACC_EN field
#define BMI270_FC1_FIFO_GYR_EN(N)			((N) << 7)	///< Puts N into the FC1_FIFO_GYR_EN field

#define BMI270_FC1_FIFO_TAG_INT1_EN_MASK	0x03	///< Mask of the FC1_FIFO_TAG_INT1_EN field
#define BMI270_FC1_FIFO_TAG_INT2_EN_MASK	0x0C	///< Mask of the FC1_FIFO_TAG_INT2_EN field
#define BMI270_FC1_FIFO_HEADER_EN_MASK		0x10	///< Mask of the FC1_FIFO_HEADER_EN field
#define BMI270_FC1_FIFO_AUX_EN_MASK			0x20	///< Mask of the FC1_FIFO_AUX_EN field
#define BMI270_FC1_FIFO_ACC_EN_MASK			0x40	///< Mask of the FC1_FIFO_ACC_EN field
#define BMI270_FC1_FIFO_GYR_EN_MASK			0x80	///< Mask of the FC1_FIFO_GYR_EN field
/** @} */


// Register : SATURATION(0x4A)

/** @name BMI270 SATURATION(0x4A) register
 *  @{
 */
#define BMI270_S_ACC_X_MASK		0x01	///< Mask of the S_ACC_X field
#define BMI270_S_ACC_Y_MASK		0x02	///< Mask of the S_ACC_Y field
#define BMI270_S_ACC_Z_MASK		0x04	///< Mask of the S_ACC_Z field
#define BMI270_S_GYR_X_MASK		0x08	///< Mask of the S_GYR_X field
#define BMI270_S_GYR_Y_MASK		0x10	///< Mask of the S_GYR_Y field
#define BMI270_S_GYR_Z_MASK		0x20	///< Mask of the S_GYR_Z field
/** @} */


// Register : AUX_DEV_ID(0x4B)

/** @name BMI270 AUX_DEV_ID(0x4B) register
 *  @{
 */
#define BMI270_ADI_I2C_DEVICE_ADDR(N)		((N) << 1)	///< Puts N into the ADI_I2C_DEVICE_ADDR field

#define BMI270_ADI_I2C_DEVICE_ADDR_MASK		0xFE	///< Mask of the ADI_I2C_DEVICE_ADDR field
/** @} */


// Register : AUX_IF_CONF(0x4C)

/** @name BMI270 AUX_IF_CONF(0x4C) register
 *  @{
 */
/**
 * @brief Bits 1-0 (aux_rd_burst): burst length of the data mode reads of the auxiliary interface.
 */
typedef enum EnPifBmi270AicAuxRdBurst
{
    BMI270_AIC_AUX_RD_BURST_BL1				= 0,
    BMI270_AIC_AUX_RD_BURST_BL2				= 1,
    BMI270_AIC_AUX_RD_BURST_BL6				= 2,
    BMI270_AIC_AUX_RD_BURST_BL8				= 3
} PifBmi270AicAuxRdBurst;

/**
 * @brief Bits 3-2 (man_rd_burst): burst length of the manual reads of the auxiliary interface.
 */
typedef enum EnPifBmi270AicManRdBurst
{
    BMI270_AIC_MAN_RD_BURST_BL1				= 0 << 2,
    BMI270_AIC_MAN_RD_BURST_BL2				= 1 << 2,
    BMI270_AIC_MAN_RD_BURST_BL6				= 2 << 2,
    BMI270_AIC_MAN_RD_BURST_BL8				= 3 << 2
} PifBmi270AicManRdBurst;

#define BMI270_AIC_AUX_FCU_WRITE_EN(N)		((N) << 6)	///< Puts N into the AIC_AUX_FCU_WRITE_EN field
#define BMI270_AIC_AUX_MANUAL_EN(N)			((N) << 7)	///< Puts N into the AIC_AUX_MANUAL_EN field

#define BMI270_AIC_AUX_RD_BURST_MASK		0x03	///< Mask of the AIC_AUX_RD_BURST field
#define BMI270_AIC_MAN_RD_BURST_MASK		0x0C	///< Mask of the AIC_MAN_RD_BURST field
#define BMI270_AIC_AUX_FCU_WRITE_EN_MASK	0x40	///< Mask of the AIC_AUX_FCU_WRITE_EN field
#define BMI270_AIC_AUX_MANUAL_EN_MASK		0x80	///< Mask of the AIC_AUX_MANUAL_EN field
/** @} */


// Register : ERR_REG_MSK(0x52)

/** @name BMI270 ERR_REG_MSK(0x52) register
 *  @{
 */
#define BMI270_ERM_FATAL_ERR(N)			(N)	///< Puts N into the ERM_FATAL_ERR field
#define BMI270_ERM_INTERNAL_ERR(N)		((N) << 1)	///< Puts N into the ERM_INTERNAL_ERR field
#define BMI270_ERM_FIFO_ERR(N)			((N) << 6)	///< Puts N into the ERM_FIFO_ERR field
#define BMI270_ERM_AUX_ERR(N)			((N) << 7)	///< Puts N into the ERM_AUX_ERR field

#define BMI270_ERM_FATAL_ERR_MASK		0x01	///< Mask of the ERM_FATAL_ERR field
#define BMI270_ERM_INTERNAL_ERR_MASK	0x1E	///< Mask of the ERM_INTERNAL_ERR field
#define BMI270_ERM_FIFO_ERR_MASK		0x40	///< Mask of the ERM_FIFO_ERR field
#define BMI270_ERM_AUX_ERR_MASK			0x80	///< Mask of the ERM_AUX_ERR field
/** @} */


// Register : INT1_IO_CTRL(0x53), INT2_IO_CTRL(0x54)

/** @name BMI270 INT1_IO_CTRL(0x53), INT2_IO_CTRL(0x54) register
 *  @{
 */
/**
 * @brief Bit 1 (lvl): active level of the interrupt pin.
 */
typedef enum EnPifBmi270IicLvl
{
    BMI270_IIC_LVL_ACTIVE_LOW		= 0 << 1,
    BMI270_IIC_LVL_ACTIVE_HIGH		= 1 << 1
} PifBmi270IicLvl;

/**
 * @brief Bit 2 (od): output type of the interrupt pin.
 */
typedef enum EnPifBmi270IicOd
{
    BMI270_IIC_OD_PUSH_PULL			= 0 << 2,
    BMI270_IIC_OD_OPEN_DRAIN		= 1 << 2
} PifBmi270IicOd;

#define BMI270_IIC_OUTPUT_EN(N)		((N) << 3)	///< Puts N into the IIC_OUTPUT_EN field
#define BMI270_IIC_INPUT_EN(N)		((N) << 4)	///< Puts N into the IIC_INPUT_EN field

#define BMI270_IIC_LVL_MASK			0x02	///< Mask of the IIC_LVL field
#define BMI270_IIC_OD_MASK			0x04	///< Mask of the IIC_OD field
#define BMI270_IIC_OUTPUT_EN_MASK	0x08	///< Mask of the IIC_OUTPUT_EN field
#define BMI270_IIC_INPUT_EN_MASK	0x10	///< Mask of the IIC_INPUT_EN field
/** @} */


// Register : INT1_MAP_FEAT(0x56), INT2_MAP_FEAT(0x57)

/** @name BMI270 INT1_MAP_FEAT(0x56), INT2_MAP_FEAT(0x57) register
 *  @{
 */
#define BMI270_IMF_SIG_MOTION_OUT(N)			(N)	///< Puts N into the IMF_SIG_MOTION_OUT field
#define BMI270_IMF_STEP_COUNTER_OUT(N)			((N) << 1)	///< Puts N into the IMF_STEP_COUNTER_OUT field
#define BMI270_IMF_ACTIVITY_OUT(N)				((N) << 2)	///< Puts N into the IMF_ACTIVITY_OUT field
#define BMI270_IMF_WRIST_WEAR_WAKEUP_OUT(N)		((N) << 3)	///< Puts N into the IMF_WRIST_WEAR_WAKEUP_OUT field
#define BMI270_IMF_WRIST_GESTURE_OUT(N)			((N) << 4)	///< Puts N into the IMF_WRIST_GESTURE_OUT field
#define BMI270_IMF_NO_MOTION_OUT(N)				((N) << 5)	///< Puts N into the IMF_NO_MOTION_OUT field
#define BMI270_IMF_ANY_MOTION_OUT(N)			((N) << 6)	///< Puts N into the IMF_ANY_MOTION_OUT field

#define BMI270_IMF_SIG_MOTION_OUT_MASK			0x01	///< Mask of the IMF_SIG_MOTION_OUT field
#define BMI270_IMF_STEP_COUNTER_OUT_MASK		0x02	///< Mask of the IMF_STEP_COUNTER_OUT field
#define BMI270_IMF_ACTIVITY_OUT_MASK			0x04	///< Mask of the IMF_ACTIVITY_OUT field
#define BMI270_IMF_WRIST_WEAR_WAKEUP_OUT_MASK	0x08	///< Mask of the IMF_WRIST_WEAR_WAKEUP_OUT field
#define BMI270_IMF_WRIST_GESTURE_OUT_MASK		0x10	///< Mask of the IMF_WRIST_GESTURE_OUT field
#define BMI270_IMF_NO_MOTION_OUT_MASK			0x20	///< Mask of the IMF_NO_MOTION_OUT field
#define BMI270_IMF_ANY_MOTION_OUT_MASK			0x40	///< Mask of the IMF_ANY_MOTION_OUT field
/** @} */


// Register : INT_MAP_DATA(0x58)

/** @name BMI270 INT_MAP_DATA(0x58) register
 *  @{
 */
#define BMI270_IMD_FFULL_INT1(N)	(N)	///< Puts N into the IMD_FFULL_INT1 field
#define BMI270_IMD_FWM_INT1(N)		((N) << 1)	///< Puts N into the IMD_FWM_INT1 field
#define BMI270_IMD_DRDY_INT1(N)		((N) << 2)	///< Puts N into the IMD_DRDY_INT1 field
#define BMI270_IMD_ERR_INT1(N)		((N) << 3)	///< Puts N into the IMD_ERR_INT1 field
#define BMI270_IMD_FFULL_INT2(N)	((N) << 4)	///< Puts N into the IMD_FFULL_INT2 field
#define BMI270_IMD_FWM_INT2(N)		((N) << 5)	///< Puts N into the IMD_FWM_INT2 field
#define BMI270_IMD_DRDY_INT2(N)		((N) << 6)	///< Puts N into the IMD_DRDY_INT2 field
#define BMI270_IMD_ERR_INT2(N)		((N) << 7)	///< Puts N into the IMD_ERR_INT2 field

#define BMI270_IMD_FFULL_INT1_MASK	0x01	///< Mask of the IMD_FFULL_INT1 field
#define BMI270_IMD_FWM_INT1_MASK	0x02	///< Mask of the IMD_FWM_INT1 field
#define BMI270_IMD_DRDY_INT1_MASK	0x04	///< Mask of the IMD_DRDY_INT1 field
#define BMI270_IMD_ERR_INT1_MASK	0x08	///< Mask of the IMD_ERR_INT1 field
#define BMI270_IMD_FFULL_INT2_MASK	0x10	///< Mask of the IMD_FFULL_INT2 field
#define BMI270_IMD_FWM_INT2_MASK	0x20	///< Mask of the IMD_FWM_INT2 field
#define BMI270_IMD_DRDY_INT2_MASK	0x40	///< Mask of the IMD_DRDY_INT2 field
#define BMI270_IMD_ERR_INT2_MASK	0x80	///< Mask of the IMD_ERR_INT2 field
/** @} */


// Register : INTERNAL_ERROR(0x5F)

/** @name BMI270 INTERNAL_ERROR(0x5F) register
 *  @{
 */
#define BMI270_IE_INT_ERR_1_MASK			0x02	///< Mask of the IE_INT_ERR_1 field
#define BMI270_IE_INT_ERR_2_MASK			0x04	///< Mask of the IE_INT_ERR_2 field
#define BMI270_IE_FEAT_ENG_DISABLED_MASK	0x10	///< Mask of the IE_FEAT_ENG_DISABLED field
/** @} */


// Register : AUX_IF_TRIM(0x68)

/** @name BMI270 AUX_IF_TRIM(0x68) register
 *  @{
 */
/**
 * @brief Bits 1-0 (asda_pupsel): pull-up of the auxiliary I2C lines.
 */
typedef enum EnPifBmi270AitAsdaPupsel
{
    BMI270_AIT_ASDA_PUPSEL_OFF			= 0,
    BMI270_AIT_ASDA_PUPSEL_40K			= 1,
    BMI270_AIT_ASDA_PUPSEL_10K			= 2,
    BMI270_AIT_ASDA_PUPSEL_2K			= 3
} PifBmi270AitAsdaPupsel;

#define BMI270_AIT_ASDA_PUPSEL_MASK		0x03	///< Mask of the AIT_ASDA_PUPSEL field
/** @} */


// Register : GYR_CRT_CONF(0x69)

/** @name BMI270 GYR_CRT_CONF(0x69) register
 *  @{
 */
#define BMI270_GCC_CRT_RUNNING(N)		((N) << 2)	///< Puts N into the GCC_CRT_RUNNING field

/**
 * @brief Bit 3 (rdy_for_dl): state of the download of the gyro CRT data.
 */
typedef enum EnPifBmi270GccRdyForDl
{
    BMI270_GCC_RDY_FOR_DL_ONGOING		= 0 << 3,
    BMI270_GCC_RDY_FOR_DL_COMPLETE		= 1 << 3,
} PifBmi270GccRdyForDl;

#define BMI270_GCC_CRT_RUNNING_MASK		0x04	///< Mask of the GCC_CRT_RUNNING field
#define BMI270_GCC_RDY_FOR_DL_MASK		0x08	///< Mask of the GCC_RDY_FOR_DL field
/** @} */


// Register : NVM_CONF(0x6A)

/** @name BMI270 NVM_CONF(0x6A) register
 *  @{
 */
#define BMI270_NC_NVM_PROG_EN(N)		((N) << 1)	///< Puts N into the NC_NVM_PROG_EN field

#define BMI270_NC_NVM_PROG_EN_MASK		0x02	///< Mask of the NC_NVM_PROG_EN field
/** @} */


// Register : IF_CONF(0x6B)

/** @name BMI270 IF_CONF(0x6B) register
 *  @{
 */
/**
 * @brief Bit 0 (spi3): 4-wire or 3-wire SPI of the primary interface.
 */
typedef enum EnPifBmi270IfSpi3
{
    BMI270_IF_SPI3_SPI4				= 0,
    BMI270_IF_SPI3_SPI3				= 1,
} PifBmi270IfSpi3;

/**
 * @brief Bit 0 (spi3): 4-wire or 3-wire SPI of the primary interface.Ois
 */
typedef enum EnPifBmi270IfSpi3Ois
{
    BMI270_IF_SPI3_OIS_SPI4			= 0 << 1,
    BMI270_IF_SPI3_OIS_SPI3			= 1 << 1,
} PifBmi270IfSpi3Ois;

#define BMI270_IF_OIS_EN(N)			((N) << 4)	///< Puts N into the IF_OIS_EN field
#define BMI270_IF_AUX_EN(N)			((N) << 5)	///< Puts N into the IF_AUX_EN field

#define BMI270_IF_SPI3_MASK			0x01	///< Mask of the IF_SPI3 field
#define BMI270_IF_SPI3_OIS_MASK		0x02	///< Mask of the IF_SPI3_OIS field
#define BMI270_IF_OIS_EN_MASK		0x10	///< Mask of the IF_OIS_EN field
#define BMI270_IF_AUX_EN_MASK		0x20	///< Mask of the IF_AUX_EN field
/** @} */


// Register : DRV(0x6C)

/** @name BMI270 DRV(0x6C) register
 *  @{
 */
#define BMI270_D_IO_PAD_DRV1(N)			(N)	///< Puts N into the D_IO_PAD_DRV1 field
#define BMI270_D_IO_PAD_I2C_B1(N)		((N) << 3)	///< Puts N into the D_IO_PAD_I2C_B1 field
#define BMI270_D_IO_PAD_DRV2(N)			((N) << 4)	///< Puts N into the D_IO_PAD_DRV2 field
#define BMI270_D_IO_PAD_I2C_B2(N)		((N) << 7)	///< Puts N into the D_IO_PAD_I2C_B2 field

#define BMI270_D_IO_PAD_DRV1_MASK		0x07	///< Mask of the D_IO_PAD_DRV1 field
#define BMI270_D_IO_PAD_I2C_B1_MASK		0x08	///< Mask of the D_IO_PAD_I2C_B1 field
#define BMI270_D_IO_PAD_DRV2_MASK		0x70	///< Mask of the D_IO_PAD_DRV2 field
#define BMI270_D_IO_PAD_I2C_B2_MASK		0x80	///< Mask of the D_IO_PAD_I2C_B2 field
/** @} */


// Register : ACC_SELF_TEST(0x6D)

/** @name BMI270 ACC_SELF_TEST(0x6D) register
 *  @{
 */
#define BMI270_AST_ACC_SELF_TEST_EN(N)		(N)	///< Puts N into the AST_ACC_SELF_TEST_EN field
#define BMI270_AST_ACC_SELF_TEST_SIGN(N)	((N) << 2)	///< Puts N into the AST_ACC_SELF_TEST_SIGN field
#define BMI270_AST_ACC_SELF_TEST_AMP(N)		((N) << 3)	///< Puts N into the AST_ACC_SELF_TEST_AMP field

#define BMI270_AST_ACC_SELF_TEST_EN_MASK	0x01	///< Mask of the AST_ACC_SELF_TEST_EN field
#define BMI270_AST_ACC_SELF_TEST_SIGN_MASK	0x04	///< Mask of the AST_ACC_SELF_TEST_SIGN field
#define BMI270_AST_ACC_SELF_TEST_AMP_MASK	0x08	///< Mask of the AST_ACC_SELF_TEST_AMP field
/** @} */


// Register : GYR_SELF_TEST_AXES(0x6E)

/** @name BMI270 GYR_SELF_TEST_AXES(0x6E) register
 *  @{
 */
#define BMI270_GSTA_GYR_ST_AXES_DONE_MASK	0x01	///< Mask of the GSTA_GYR_ST_AXES_DONE field
#define BMI270_GSTA_GYR_AXIS_X_OK_MASK		0x02	///< Mask of the GSTA_GYR_AXIS_X_OK field
#define BMI270_GSTA_GYR_AXIS_Y_OK_MASK		0x04	///< Mask of the GSTA_GYR_AXIS_Y_OK field
#define BMI270_GSTA_GYR_AXIS_Z_OK_MASK		0x08	///< Mask of the GSTA_GYR_AXIS_Z_OK field
/** @} */


// Register : NV_CONF(0x70)

/** @name BMI270 NV_CONF(0x70) register
 *  @{
 */
#define BMI270_NC_SPI_EN(N)				(N)	///< Puts N into the NC_SPI_EN field

/**
 * @brief Bit 1 (i2c_wdt_sel): timeout of the I2C watchdog.
 */
typedef enum EnPifBmi270NcI2cWdtSel
{
    BMI270_NC_I2C_WDT_SEL_SHORT			= 0 << 1,
    BMI270_NC_I2C_WDT_SEL_LONGG			= 1 << 1,
} PifBmi270NcI2cWdtSel;

#define BMI270_NC_I2C_WDT_EN(N)			((N) << 2)	///< Puts N into the NC_I2C_WDT_EN field
#define BMI270_NC_ACC_OFF_EN(N)			((N) << 3)	///< Puts N into the NC_ACC_OFF_EN field

#define BMI270_NC_SPI_EN_MASK			0x01	///< Mask of the NC_SPI_EN field
#define BMI270_NC_I2C_WDT_SEL_MASK		0x02	///< Mask of the NC_I2C_WDT_SEL field
#define BMI270_NC_I2C_WDT_EN_MASK		0x04	///< Mask of the NC_I2C_WDT_EN field
#define BMI270_NC_ACC_OFF_EN_MASK		0x08	///< Mask of the NC_ACC_OFF_EN field
/** @} */


// Register : OFFSET_6(0x77)

/** @name BMI270 OFFSET_6(0x77) register
 *  @{
 */
#define BMI270_O6_GYR_USR_OFF_X_HSB(N)		(N)	///< Puts N into the O6_GYR_USR_OFF_X_HSB field
#define BMI270_O6_GYR_USR_OFF_Y_HSB(N)		((N) << 2)	///< Puts N into the O6_GYR_USR_OFF_Y_HSB field
#define BMI270_O6_GYR_USR_OFF_Z_HSB(N)		((N) << 4)	///< Puts N into the O6_GYR_USR_OFF_Z_HSB field
#define BMI270_O6_GYR_OFF_EN(N)				((N) << 6)	///< Puts N into the O6_GYR_OFF_EN field
#define BMI270_O6_GYR_GAIN_EN(N)			((N) << 7)	///< Puts N into the O6_GYR_GAIN_EN field

#define BMI270_O6_GYR_USR_OFF_X_HSB_MASK	0x03	///< Mask of the O6_GYR_USR_OFF_X_HSB field
#define BMI270_O6_GYR_USR_OFF_Y_HSB_MASK	0x0C	///< Mask of the O6_GYR_USR_OFF_Y_HSB field
#define BMI270_O6_GYR_USR_OFF_Z_HSB_MASK	0x30	///< Mask of the O6_GYR_USR_OFF_Z_HSB field
#define BMI270_O6_GYR_OFF_EN_MASK			0x40	///< Mask of the O6_GYR_OFF_EN field
#define BMI270_O6_GYR_GAIN_EN_MASK			0x80	///< Mask of the O6_GYR_GAIN_EN field
/** @} */


// Register : PWR_CONF(0x7C)

/** @name BMI270 PWR_CONF(0x7C) register
 *  @{
 */
#define BMI270_PC_ADV_POWER_SAVE(N)			(N)	///< Puts N into the PC_ADV_POWER_SAVE field
#define BMI270_PC_FIFO_SELF_WAKE_UP(N)		((N) << 1)	///< Puts N into the PC_FIFO_SELF_WAKE_UP field
#define BMI270_PC_FUP_EN(N)					((N) << 2)	///< Puts N into the PC_FUP_EN field

#define BMI270_PC_ADV_POWER_SAVE_MASK		0x01	///< Mask of the PC_ADV_POWER_SAVE field
#define BMI270_PC_FIFO_SELF_WAKE_UP_MASK	0x02	///< Mask of the PC_FIFO_SELF_WAKE_UP field
#define BMI270_PC_FUP_EN_MASK				0x04	///< Mask of the PC_FUP_EN field
/** @} */


// Register : PWR_CTRL(0x7D)

/** @name BMI270 PWR_CTRL(0x7D) register
 *  @{
 */
#define BMI270_PC_AUX_EN(N)			(N)	///< Puts N into the PC_AUX_EN field
#define BMI270_PC_GYR_EN(N)			((N) << 1)	///< Puts N into the PC_GYR_EN field
#define BMI270_PC_ACC_EN(N)			((N) << 2)	///< Puts N into the PC_ACC_EN field
#define BMI270_PC_TEMP_EN(N)		((N) << 3)	///< Puts N into the PC_TEMP_EN field

#define BMI270_PC_AUX_EN_MASK		0x01	///< Mask of the PC_AUX_EN field
#define BMI270_PC_GYR_EN_MASK		0x02	///< Mask of the PC_GYR_EN field
#define BMI270_PC_ACC_EN_MASK		0x04	///< Mask of the PC_ACC_EN field
#define BMI270_PC_TEMP_EN_MASK		0x08	///< Mask of the PC_TEMP_EN field
/** @} */


// Register : CMD(0x7E)

/** @name BMI270 CMD(0x7E) register
 *  @{
 */
/**
 * @brief Commands written to the CMD register.
 */
typedef enum EnPifBmi270CCmd
{
    BMI270_C_CMD_G_TRIGGER		= 0x02,
    BMI270_C_CMD_USR_GAIN		= 0x03,
    BMI270_C_CMD_NVM_PROG		= 0xA0,
    BMI270_C_CMD_FIFO_FLUSH		= 0xB0,
    BMI270_C_CMD_SOFT_RESET		= 0xB6
} PifBmi270CCmd;
/** @} */


/**
 * @class StPifBmi270
 * @brief Bosch BMI270 gyroscope and accelerometer.
 */
typedef struct StPifBmi270
{
	// Public Member Variable
	uint8_t gyro_scale;	///< Divider of the raw gyro values, 0 for none
	uint8_t accel_scale;	///< Divider of the raw accelerometer values, 0 for none
	uint8_t temp_scale;	///< Multiplier of the temperature

	// Read-only Member Variable
	PifId _id;						///< Instance ID
	PifDeviceReg *_p_device;		///< I2C or SPI device of the sensor


	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
	BOOL __dummy_read;				///< Over SPI every read starts with a byte that is not data
} PifBmi270;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resets the device, checks its chip ID, loads the configuration file and turns the
 *        accelerometer, the gyroscope and the temperature sensor on in performance mode
 *        (accelerometer 100Hz, gyroscope 200Hz). It holds the CPU for some tens of ms.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270_Config(PifBmi270* p_owner, PifId id, PifImuSensor* p_imu_sensor);

/**
 * @brief Loads the configuration file into the feature engine, in pieces of the device's
 *        max_transfer_size when it has one, and waits for INTERNAL_STATUS to report it taken.
 *        pifBmi270_Config() already does this.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE on failure or when the engine does not report init OK.
 */
BOOL pifBmi270_UploadConfig(PifBmi270 *p_owner);

/**
 * @brief Sets the gyroscope range and the matching gain of the IMU sensor.
 * @param p_owner Pointer to the owner instance.
 * @param range Gyroscope range.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270_SetGyroRange(PifBmi270* p_owner, PifBmi270GrGyrRange range);

/**
 * @brief Sets the accelerometer range and the matching gain of the IMU sensor.
 * @param p_owner Pointer to the owner instance.
 * @param range Accelerometer range.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270_SetAccelRange(PifBmi270* p_owner, PifBmi270ArAccRange range);

/**
 * @brief Reads the raw gyro values of the 3 axes, divided by gyro_scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270_ReadGyro(PifBmi270* p_owner, int16_t* p_gyro);

/**
 * @brief Reads the raw accelerometer values of the 3 axes, divided by accel_scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270_ReadAccel(PifBmi270* p_owner, int16_t* p_accel);

/**
 * @brief Reads the temperature in degrees C, multiplied by temp_scale.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Receives the temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270_ReadTemperature(PifBmi270* p_owner, int16_t* p_temperature);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_H
