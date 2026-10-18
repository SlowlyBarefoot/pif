// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_mpu60x0.h
 * @brief InvenSense MPU-6000 and MPU-6050 6-axis gyroscope and accelerometer, as a PifImuSensor.
 *
 * pif_mpu60x0_i2c and pif_mpu60x0_spi attach the device. The register map below follows the register map
 * document of the chip.
 */
#ifndef PIF_MPU60X0_H
#define PIF_MPU60X0_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "sensor/pif_imu_sensor.h"


#define MPU60X0_I2C_ADDR(N)			(0x68 + (N))	///< I2C address with AD0 on GND (0) or VDD (1)

#define MPU60X0_WHO_AM_I_CONST		0x68		///< Value of the WHO_AM_I register


/**
 * @brief Register addresses, named as in the register map document.
 */
typedef enum EnPifMpu60x0Reg
{
	MPU60X0_REG_PRODUCT_ID       	= 0x0C,		///< Not in the register map, but read by the MPU-6000 drivers to tell its revisions apart
	MPU60X0_REG_SELF_TEST_X     	= 0x0D,
	MPU60X0_REG_SELF_TEST_Y      	= 0x0E,
	MPU60X0_REG_SELF_TEST_Z     	= 0x0F,
	MPU60X0_REG_SELF_TEST_A      	= 0x10,
	MPU60X0_REG_SMPLRT_DIV      	= 0x19,
	MPU60X0_REG_CONFIG         	  	= 0x1A,
	MPU60X0_REG_GYRO_CONFIG     	= 0x1B,
	MPU60X0_REG_ACCEL_CONFIG    	= 0x1C,
	MPU60X0_REG_FIFO_EN        	  	= 0x23,
	MPU60X0_REG_I2C_MST_CTRL   	  	= 0x24,
	MPU60X0_REG_I2C_SLV0_ADDR  	  	= 0x25,
	MPU60X0_REG_I2C_SLV0_REG   	  	= 0x26,
	MPU60X0_REG_I2C_SLV0_CTRL    	= 0x27,
	MPU60X0_REG_I2C_SLV1_ADDR   	= 0x28,
	MPU60X0_REG_I2C_SLV1_REG    	= 0x29,
	MPU60X0_REG_I2C_SLV1_CTRL  	  	= 0x2A,
	MPU60X0_REG_I2C_SLV2_ADDR  	  	= 0x2B,
	MPU60X0_REG_I2C_SLV2_REG   	  	= 0x2C,
	MPU60X0_REG_I2C_SLV2_CTRL  	  	= 0x2D,
	MPU60X0_REG_I2C_SLV3_ADDR  	  	= 0x2E,
	MPU60X0_REG_I2C_SLV3_REG    	= 0x2F,
	MPU60X0_REG_I2C_SLV3_CTRL   	= 0x30,
	MPU60X0_REG_I2C_SLV4_ADDR   	= 0x31,
	MPU60X0_REG_I2C_SLV4_REG    	= 0x32,
	MPU60X0_REG_I2C_SLV4_DO     	= 0x33,
	MPU60X0_REG_I2C_SLV4_CTRL   	= 0x34,
	MPU60X0_REG_I2C_SLV4_DI     	= 0x35,
	MPU60X0_REG_I2C_MST_STATUS  	= 0x36,
	MPU60X0_REG_INT_PIN_CFG      	= 0x37,
	MPU60X0_REG_INT_ENABLE       	= 0x38,
	MPU60X0_REG_INT_STATUS       	= 0x3A,
	MPU60X0_REG_ACCEL_XOUT_H    	= 0x3B,
	MPU60X0_REG_ACCEL_XOUT_L     	= 0x3C,
	MPU60X0_REG_ACCEL_YOUT_H     	= 0x3D,
	MPU60X0_REG_ACCEL_YOUT_L     	= 0x3E,
	MPU60X0_REG_ACCEL_ZOUT_H    	= 0x3F,
	MPU60X0_REG_ACCEL_ZOUT_L    	= 0x40,
	MPU60X0_REG_TEMP_OUT_H      	= 0x41,
	MPU60X0_REG_TEMP_OUT_L      	= 0x42,
	MPU60X0_REG_GYRO_XOUT_H     	= 0x43,
	MPU60X0_REG_GYRO_XOUT_L     	= 0x44,
	MPU60X0_REG_GYRO_YOUT_H     	= 0x45,
	MPU60X0_REG_GYRO_YOUT_L      	= 0x46,
	MPU60X0_REG_GYRO_ZOUT_H     	= 0x47,
	MPU60X0_REG_GYRO_ZOUT_L     	= 0x48,
	MPU60X0_REG_EXT_SENS_DATA_00	= 0x49,
	MPU60X0_REG_EXT_SENS_DATA_01	= 0x4A,
	MPU60X0_REG_EXT_SENS_DATA_02	= 0x4B,
	MPU60X0_REG_EXT_SENS_DATA_03	= 0x4C,
	MPU60X0_REG_EXT_SENS_DATA_04	= 0x4D,
	MPU60X0_REG_EXT_SENS_DATA_05	= 0x4E,
	MPU60X0_REG_EXT_SENS_DATA_06	= 0x4F,
	MPU60X0_REG_EXT_SENS_DATA_07	= 0x50,
	MPU60X0_REG_EXT_SENS_DATA_08	= 0x51,
	MPU60X0_REG_EXT_SENS_DATA_09	= 0x52,
	MPU60X0_REG_EXT_SENS_DATA_10	= 0x53,
	MPU60X0_REG_EXT_SENS_DATA_11	= 0x54,
	MPU60X0_REG_EXT_SENS_DATA_12	= 0x55,
	MPU60X0_REG_EXT_SENS_DATA_13	= 0x56,
	MPU60X0_REG_EXT_SENS_DATA_14	= 0x57,
	MPU60X0_REG_EXT_SENS_DATA_15	= 0x58,
	MPU60X0_REG_EXT_SENS_DATA_16	= 0x59,
	MPU60X0_REG_EXT_SENS_DATA_17	= 0x5A,
	MPU60X0_REG_EXT_SENS_DATA_18	= 0x5B,
	MPU60X0_REG_EXT_SENS_DATA_19	= 0x5C,
	MPU60X0_REG_EXT_SENS_DATA_20	= 0x5D,
	MPU60X0_REG_EXT_SENS_DATA_21	= 0x5E,
	MPU60X0_REG_EXT_SENS_DATA_22	= 0x5F,
	MPU60X0_REG_EXT_SENS_DATA_23	= 0x60,
	MPU60X0_REG_I2C_SLV0_DO     	= 0x63,
	MPU60X0_REG_I2C_SLV1_DO     	= 0x64,
	MPU60X0_REG_I2C_SLV2_DO     	= 0x65,
	MPU60X0_REG_I2C_SLV3_DO     	= 0x66,
	MPU60X0_REG_I2C_MST_DELAY_CTRL	= 0x67,
	MPU60X0_REG_SIGNAL_PATH_RESET  	= 0x68,
	MPU60X0_REG_USER_CTRL       	= 0x6A,
	MPU60X0_REG_PWR_MGMT_1      	= 0x6B,
	MPU60X0_REG_PWR_MGMT_2      	= 0x6C,
	MPU60X0_REG_FIFO_COUNTH     	= 0x72,
	MPU60X0_REG_FIFO_COUNTL     	= 0x73,
	MPU60X0_REG_FIFO_R_W        	= 0x74,
	MPU60X0_REG_WHO_AM_I        	= 0x75
} PifMpu60x0Reg;


// Register : SELF_TEST_X, SELF_TEST_Y, SELF_TEST_Z

/** @name MPU60X0 SELF_TEST_X, SELF_TEST_Y, SELF_TEST_Z register
 *  @{
 */
#define MPU60X0_G_TEST(N)		(N)	///< Puts N into the G_TEST field
#define MPU60X0_A_TEST(N)		((N) << 5)	///< Puts N into the A_TEST field

#define MPU60X0_G_TEST_MASK		0x1F	///< Mask of the G_TEST field
#define MPU60X0_A_TEST_MASK		0xE0	///< Mask of the A_TEST field
/** @} */


// Register : SELF_TEST_A

/** @name MPU60X0 SELF_TEST_A register
 *  @{
 */
#define MPU60X0_ZA_TEST(N)		(N)	///< Puts N into the ZA_TEST field
#define MPU60X0_YA_TEST(N)		((N) << 2)	///< Puts N into the YA_TEST field
#define MPU60X0_XA_TEST(N)		((N) << 4)	///< Puts N into the XA_TEST field

#define MPU60X0_ZA_TEST_MASK	0x03	///< Mask of the ZA_TEST field
#define MPU60X0_YA_TEST_MASK	0x0C	///< Mask of the YA_TEST field
#define MPU60X0_XA_TEST_MASK	0x30	///< Mask of the XA_TEST field
/** @} */


// Register : CONFIG

/** @name MPU60X0 CONFIG register
 *  @{
 */
/**
 * @brief Bits 2-0 (DLPF_CFG): bandwidth of the digital low pass filter of the accelerometer and the gyro.
 */
typedef enum EnPifMpu60x0DlpfCfg
{
	MPU60X0_DLPF_CFG_A260HZ_G256HZ		= 0,	///< This is the default setting, no need to uncomment, just for reference
	MPU60X0_DLPF_CFG_A184HZ_G188HZ		= 1,
	MPU60X0_DLPF_CFG_A94HZ_G98HZ		= 2,
	MPU60X0_DLPF_CFG_A44HZ_G42HZ		= 3,
	MPU60X0_DLPF_CFG_A21HZ_G20HZ		= 4,
	MPU60X0_DLPF_CFG_A10HZ_G10HZ		= 5,
    MPU60X0_DLPF_CFG_A5HZ_G5HZ      	= 6		///< Use this only in extreme cases, rather change motors and/or props
} PifMpu60x0DlpfCfg;

/**
 * @brief Bits 5-3 (EXT_SYNC_SET): sample whose LSB carries the FSYNC input.
 */
typedef enum EnPifMpu60x0ExtSyncSet
{
	MPU60X0_EXT_SYNC_SET_INPUT_DISABLE	= 0 << 3,
	MPU60X0_EXT_SYNC_SET_TEMP_OUT		= 1 << 3,
	MPU60X0_EXT_SYNC_SET_GYRO_XOUT		= 2 << 3,
	MPU60X0_EXT_SYNC_SET_GYRO_YOUT		= 3 << 3,
	MPU60X0_EXT_SYNC_SET_GYRO_ZOUT		= 4 << 3,
	MPU60X0_EXT_SYNC_SET_ACCEL_XOUT		= 5 << 3,
    MPU60X0_EXT_SYNC_SET_ACCEL_YOUT		= 6 << 3,
    MPU60X0_EXT_SYNC_SET_ACCEL_ZOUT		= 7 << 3
} PifMpu60x0ExtSyncSet;

#define MPU60X0_DLPF_CFG_MASK			0x07	///< Mask of the DLPF_CFG field
#define MPU60X0_EXT_SYNC_SET_MASK		0x38	///< Mask of the EXT_SYNC_SET field
/** @} */


// Register : GYRO_CONFIG

/** @name MPU60X0 GYRO_CONFIG register
 *  @{
 */
/**
 * @brief Bits 4-3 (FS_SEL): full scale range of the gyro.
 */
typedef enum EnPifMpu60x0FsSel
{
    MPU60X0_FS_SEL_250DPS		= 0 << 3,	///< Default
    MPU60X0_FS_SEL_500DPS		= 1 << 3,
    MPU60X0_FS_SEL_1000DPS		= 2 << 3,
    MPU60X0_FS_SEL_2000DPS		= 3 << 3,

    MPU60X0_FS_SEL_DEFAULT	 	= 0 << 3
} PifMpu60x0FsSel;

#define MPU60X0_ZG_ST(N)		((N) << 5)	///< Puts N into the ZG_ST field
#define MPU60X0_YG_ST(N)		((N) << 6)	///< Puts N into the YG_ST field
#define MPU60X0_XG_ST(N)		((N) << 7)	///< Puts N into the XG_ST field

#define MPU60X0_FS_SEL_MASK		0x18		///< Use pifMpu60x0_SetGyroConfig or pifMpu60x0_SetFsSel to change this value.
#define MPU60X0_ZG_ST_MASK		0x20		///< Mask of the ZG_ST field
#define MPU60X0_YG_ST_MASK		0x40		///< Mask of the YG_ST field
#define MPU60X0_XG_ST_MASK		0x80		///< Mask of the XG_ST field
/** @} */


// Register : ACCEL_CONFIG

/** @name MPU60X0 ACCEL_CONFIG register
 *  @{
 */
/**
 * @brief Bits 4-3 (AFS_SEL): full scale range of the accelerometer.
 */
typedef enum EnPifMpu60x0AfsSel
{
    MPU60X0_AFS_SEL_2G			= 0 << 3,	///< Default
    MPU60X0_AFS_SEL_4G			= 1 << 3,
    MPU60X0_AFS_SEL_8G			= 2 << 3,
    MPU60X0_AFS_SEL_16G			= 3 << 3,

    MPU60X0_AFS_SEL_DEFAULT		= 0 << 3
} PifMpu60x0AfsSel;

#define MPU60X0_ZA_ST(N)		((N) << 5)	///< Puts N into the ZA_ST field
#define MPU60X0_YA_ST(N)		((N) << 6)	///< Puts N into the YA_ST field
#define MPU60X0_XA_ST(N)		((N) << 7)	///< Puts N into the XA_ST field

#define MPU60X0_AFS_SEL_MASK	0x18		///< Use pifMpu60x0_SetAccelConfig or pifMpu60x0_SetAfsSel to change this value.
#define MPU60X0_ZA_ST_MASK		0x20		///< Mask of the ZA_ST field
#define MPU60X0_YA_ST_MASK		0x40		///< Mask of the YA_ST field
#define MPU60X0_XA_ST_MASK		0x80		///< Mask of the XA_ST field
/** @} */


// Register : FIFO_EN

/** @name MPU60X0 FIFO_EN register
 *  @{
 */
#define MPU60X0_SLV0_FIFO_EN(N)		(N)	///< Puts N into the SLV0_FIFO_EN field
#define MPU60X0_SLV1_FIFO_EN(N)		((N) << 1)	///< Puts N into the SLV1_FIFO_EN field
#define MPU60X0_SLV2_FIFO_EN(N)		((N) << 2)	///< Puts N into the SLV2_FIFO_EN field
#define MPU60X0_ACCEL_FIFO_EN(N)	((N) << 3)	///< Puts N into the ACCEL_FIFO_EN field
#define MPU60X0_ZG_FIFO_EN(N)		((N) << 4)	///< Puts N into the ZG_FIFO_EN field
#define MPU60X0_YG_FIFO_EN(N)		((N) << 5)	///< Puts N into the YG_FIFO_EN field
#define MPU60X0_XG_FIFO_EN(N)		((N) << 6)	///< Puts N into the XG_FIFO_EN field
#define MPU60X0_TEMP_FIFO_EN(N)		((N) << 7)	///< Puts N into the TEMP_FIFO_EN field

#define MPU60X0_SLV0_FIFO_EN_MASK	0x01	///< Mask of the SLV0_FIFO_EN field
#define MPU60X0_SLV1_FIFO_EN_MASK	0x02	///< Mask of the SLV1_FIFO_EN field
#define MPU60X0_SLV2_FIFO_EN_MASK	0x04	///< Mask of the SLV2_FIFO_EN field
#define MPU60X0_ACCEL_FIFO_EN_MASK	0x08	///< Mask of the ACCEL_FIFO_EN field
#define MPU60X0_ZG_FIFO_EN_MASK		0x10	///< Mask of the ZG_FIFO_EN field
#define MPU60X0_YG_FIFO_EN_MASK		0x20	///< Mask of the YG_FIFO_EN field
#define MPU60X0_XG_FIFO_EN_MASK		0x40	///< Mask of the XG_FIFO_EN field
#define MPU60X0_TEMP_FIFO_EN_MASK	0x80	///< Mask of the TEMP_FIFO_EN field
/** @} */


// Register : I2C_MST_CTRL

/** @name MPU60X0 I2C_MST_CTRL register
 *  @{
 */
/**
 * @brief Bits 3-0 (I2C_MST_CLK): clock of the auxiliary I2C master.
 */
typedef enum EnPifMpu60x0I2cMstClk
{
    MPU60X0_I2C_MST_CLK_348KHZ		= 0,	///< Default
    MPU60X0_I2C_MST_CLK_333KHZ		= 1,
    MPU60X0_I2C_MST_CLK_320KHZ		= 2,
    MPU60X0_I2C_MST_CLK_308KHZ		= 3,
    MPU60X0_I2C_MST_CLK_296KHZ		= 4,
    MPU60X0_I2C_MST_CLK_286KHZ		= 5,
    MPU60X0_I2C_MST_CLK_276KHZ		= 6,
    MPU60X0_I2C_MST_CLK_267KHZ		= 7,
    MPU60X0_I2C_MST_CLK_258KHZ		= 8,
    MPU60X0_I2C_MST_CLK_500KHZ		= 9,
    MPU60X0_I2C_MST_CLK_471KHZ		= 10,
    MPU60X0_I2C_MST_CLK_444KHZ		= 11,
    MPU60X0_I2C_MST_CLK_421KHZ		= 12,
    MPU60X0_I2C_MST_CLK_400KHZ		= 13,
    MPU60X0_I2C_MST_CLK_381KHZ		= 14,
    MPU60X0_I2C_MST_CLK_364KHZ		= 15,

    MPU60X0_I2C_MST_CLK_DEFAULT		= 0
} PifMpu60x0I2cMstClk;

#define MPU60X0_I2C_MST_P_NSR(N)	((N) << 4)	///< Puts N into the I2C_MST_P_NSR field
#define MPU60X0_SLV3_FIFO_EN(N)		((N) << 5)	///< Puts N into the SLV3_FIFO_EN field
#define MPU60X0_WAIT_FOR_ES(N)		((N) << 6)	///< Puts N into the WAIT_FOR_ES field
#define MPU60X0_MULT_MST_EN(N)		((N) << 7)	///< Puts N into the MULT_MST_EN field

#define MPU60X0_I2C_MST_CLK_MASK	0x0F	///< Mask of the I2C_MST_CLK field
#define MPU60X0_I2C_MST_P_NSR_MASK	0x10	///< Mask of the I2C_MST_P_NSR field
#define MPU60X0_SLV3_FIFO_EN_MASK	0x20	///< Mask of the SLV3_FIFO_EN field
#define MPU60X0_WAIT_FOR_ES_MASK	0x40	///< Mask of the WAIT_FOR_ES field
#define MPU60X0_MULT_MST_EN_MASK	0x80	///< Mask of the MULT_MST_EN field
/** @} */


// Register : I2C_SLVx_ADDR

/** @name MPU60X0 I2C_SLVx_ADDR register
 *  @{
 */
#define MPU60X0_I2C_SLV_ADDR(N)		(N)	///< Puts N into the I2C_SLV_ADDR field
#define MPU60X0_I2C_SLV_RW(N)		((N) << 7)	///< Puts N into the I2C_SLV_RW field

#define MPU60X0_I2C_SLV_ADDR_MASK	0x7F	///< Mask of the I2C_SLV_ADDR field
#define MPU60X0_I2C_SLV_RW_MASK		0x80	///< Mask of the I2C_SLV_RW field
/** @} */


// Register : I2C_SLVx_CTRL

/** @name MPU60X0 I2C_SLVx_CTRL register
 *  @{
 */
#define MPU60X0_I2C_SLV_LEN(N)			(N)	///< Puts N into the I2C_SLV_LEN field
#define MPU60X0_I2C_SLV_GRP(N)			((N) << 4)	///< Puts N into the I2C_SLV_GRP field
#define MPU60X0_I2C_SLV_REG_DIS(N)		((N) << 5)	///< Puts N into the I2C_SLV_REG_DIS field
#define MPU60X0_I2C_SLV_BYTE_SW(N)		((N) << 6)	///< Puts N into the I2C_SLV_BYTE_SW field
#define MPU60X0_I2C_SLV_EN(N)			((N) << 7)	///< Puts N into the I2C_SLV_EN field

#define MPU60X0_I2C_SLV_LEN_MASK		0x0F	///< Mask of the I2C_SLV_LEN field
#define MPU60X0_I2C_SLV_GRP_MASK		0x10	///< Mask of the I2C_SLV_GRP field
#define MPU60X0_I2C_SLV_REG_DIS_MASK	0x20	///< Mask of the I2C_SLV_REG_DIS field
#define MPU60X0_I2C_SLV_BYTE_SW_MASK	0x40	///< Mask of the I2C_SLV_BYTE_SW field
#define MPU60X0_I2C_SLV_EN_MASK			0x80	///< Mask of the I2C_SLV_EN field
/** @} */


// Register : I2C_MST_STSTUS

/** @name MPU60X0 I2C_MST_STATUS register
 *  @{
 */
#define MPU60X0_I2C_SLV0_NACK(N)	(N)	///< Puts N into the I2C_SLV0_NACK field
#define MPU60X0_I2C_SLV1_NACK(N)	((N) << 1)	///< Puts N into the I2C_SLV1_NACK field
#define MPU60X0_I2C_SLV2_NACK(N)	((N) << 2)	///< Puts N into the I2C_SLV2_NACK field
#define MPU60X0_I2C_SLV3_NACK(N)	((N) << 3)	///< Puts N into the I2C_SLV3_NACK field
#define MPU60X0_I2C_SLV4_NACK(N)	((N) << 4)	///< Puts N into the I2C_SLV4_NACK field
#define MPU60X0_I2C_LOST_ARB(N)		((N) << 5)	///< Puts N into the I2C_LOST_ARB field
#define MPU60X0_I2C_SLV4_DONE(N)	((N) << 6)	///< Puts N into the I2C_SLV4_DONE field
#define MPU60X0_PASS_THROUGH(N)		((N) << 7)	///< Puts N into the PASS_THROUGH field

#define MPU60X0_I2C_SLV0_NACK_MASK	0x01	///< Mask of the I2C_SLV0_NACK field
#define MPU60X0_I2C_SLV1_NACK_MASK	0x02	///< Mask of the I2C_SLV1_NACK field
#define MPU60X0_I2C_SLV2_NACK_MASK	0x04	///< Mask of the I2C_SLV2_NACK field
#define MPU60X0_I2C_SLV3_NACK_MASK	0x08	///< Mask of the I2C_SLV3_NACK field
#define MPU60X0_I2C_SLV4_NACK_MASK	0x10	///< Mask of the I2C_SLV4_NACK field
#define MPU60X0_I2C_LOST_ARB_MASK	0x20	///< Mask of the I2C_LOST_ARB field
#define MPU60X0_I2C_SLV4_DONE_MASK	0x40	///< Mask of the I2C_SLV4_DONE field
#define MPU60X0_PASS_THROUGH_MASK	0x80	///< Mask of the PASS_THROUGH field
/** @} */


// Register : INT_PIN_CFG

/** @name MPU60X0 INT_PIN_CFG register
 *  @{
 */
#define MPU60X0_I2C_BYPASS_EN(N)		((N) << 1)	///< Puts N into the I2C_BYPASS_EN field
#define MPU60X0_FSYNC_INT_EN(N)			((N) << 2)	///< Puts N into the FSYNC_INT_EN field
#define MPU60X0_FSYNC_INT_LEVEL(N)		((N) << 3)	///< Puts N into the FSYNC_INT_LEVEL field
#define MPU60X0_INT_RD_CLEAR(N)			((N) << 4)	///< Puts N into the INT_RD_CLEAR field
#define MPU60X0_LATCH_INT_EN(N)			((N) << 5)	///< Puts N into the LATCH_INT_EN field
#define MPU60X0_INT_OPEN(N)				((N) << 6)	///< Puts N into the INT_OPEN field
#define MPU60X0_INT_LEVEL(N)			((N) << 7)	///< Puts N into the INT_LEVEL field

#define MPU60X0_I2C_BYPASS_EN_MASK		0x02	///< Mask of the I2C_BYPASS_EN field
#define MPU60X0_FSYNC_INT_EN_MASK		0x04	///< Mask of the FSYNC_INT_EN field
#define MPU60X0_FSYNC_INT_LEVEL_MASK	0x08	///< Mask of the FSYNC_INT_LEVEL field
#define MPU60X0_INT_RD_CLEAR_MASK		0x10	///< Mask of the INT_RD_CLEAR field
#define MPU60X0_LATCH_INT_EN_MASK		0x20	///< Mask of the LATCH_INT_EN field
#define MPU60X0_INT_OPEN_MASK			0x40	///< Mask of the INT_OPEN field
#define MPU60X0_INT_LEVEL_MASK			0x80	///< Mask of the INT_LEVEL field
/** @} */


// Register : INT_ENABLE

/** @name MPU60X0 INT_ENABLE register
 *  @{
 */
#define MPU60X0_DATA_RDY_EN(N)			(N)	///< Puts N into the DATA_RDY_EN field
#define MPU60X0_I2C_MST_INT_EN(N)		((N) << 3)	///< Puts N into the I2C_MST_INT_EN field
#define MPU60X0_FIFO_OFLOW_EN(N)		((N) << 4)	///< Puts N into the FIFO_OFLOW_EN field

#define MPU60X0_DATA_RDY_EN_MASK		0x01	///< Mask of the DATA_RDY_EN field
#define MPU60X0_I2C_MST_INT_EN_MASK		0x08	///< Mask of the I2C_MST_INT_EN field
#define MPU60X0_FIFO_OFLOW_EN_MASK		0x10	///< Mask of the FIFO_OFLOW_EN field
/** @} */


// Register : INT_STATUS

/** @name MPU60X0 INT_STATUS register
 *  @{
 */
#define MPU60X0_DATA_RDY_INT(N)			(N)	///< Puts N into the DATA_RDY_INT field
#define MPU60X0_I2C_MST_INT(N)			((N) << 3)	///< Puts N into the I2C_MST_INT field
#define MPU60X0_FIFO_OFLOW_INT(N)		((N) << 4)	///< Puts N into the FIFO_OFLOW_INT field

#define MPU60X0_DATA_RDY_INT_MASK		0x01	///< Mask of the DATA_RDY_INT field
#define MPU60X0_I2C_MST_INT_MASK		0x08	///< Mask of the I2C_MST_INT field
#define MPU60X0_FIFO_OFLOW_INT_MASK		0x10	///< Mask of the FIFO_OFLOW_INT field
/** @} */


// Register : I2C_MST_DELAY_CTRL

/** @name MPU60X0 I2C_MST_DELAY_CTRL register
 *  @{
 */
#define MPU60X0_I2C_SLV0_DLY_EN(N)		(N)	///< Puts N into the I2C_SLV0_DLY_EN field
#define MPU60X0_I2C_SLV1_DLY_EN(N)		((N) << 1)	///< Puts N into the I2C_SLV1_DLY_EN field
#define MPU60X0_I2C_SLV2_DLY_EN(N)		((N) << 2)	///< Puts N into the I2C_SLV2_DLY_EN field
#define MPU60X0_I2C_SLV3_DLY_EN(N)		((N) << 3)	///< Puts N into the I2C_SLV3_DLY_EN field
#define MPU60X0_I2C_SLV4_DLY_EN(N)		((N) << 4)	///< Puts N into the I2C_SLV4_DLY_EN field
#define MPU60X0_DELAY_ES_SHADOW(N)		((N) << 7)	///< Puts N into the DELAY_ES_SHADOW field

#define MPU60X0_I2C_SLV0_DLY_EN_MASK	0x01	///< Mask of the I2C_SLV0_DLY_EN field
#define MPU60X0_I2C_SLV1_DLY_EN_MASK	0x02	///< Mask of the I2C_SLV1_DLY_EN field
#define MPU60X0_I2C_SLV2_DLY_EN_MASK	0x04	///< Mask of the I2C_SLV2_DLY_EN field
#define MPU60X0_I2C_SLV3_DLY_EN_MASK	0x08	///< Mask of the I2C_SLV3_DLY_EN field
#define MPU60X0_I2C_SLV4_DLY_EN_MASK	0x10	///< Mask of the I2C_SLV4_DLY_EN field
#define MPU60X0_DELAY_ES_SHADOW_MASK	0x80	///< Mask of the DELAY_ES_SHADOW field
/** @} */


// Register : SIGNAL_PATH_RESET

/** @name MPU60X0 SIGNAL_PATH_RESET register
 *  @{
 */
#define MPU60X0_TEMP_RESET(N)		(N)	///< Puts N into the TEMP_RESET field
#define MPU60X0_ACCEL_RESET(N)		((N) << 1)	///< Puts N into the ACCEL_RESET field
#define MPU60X0_GYRO_RESET(N)		((N) << 2)	///< Puts N into the GYRO_RESET field

#define MPU60X0_TEMP_RESET_MASK		0x01	///< Mask of the TEMP_RESET field
#define MPU60X0_ACCEL_RESET_MASK	0x02	///< Mask of the ACCEL_RESET field
#define MPU60X0_GYRO_RESET_MASK		0x04	///< Mask of the GYRO_RESET field
/** @} */


// Register : USER_CTRL

/** @name MPU60X0 USER_CTRL register
 *  @{
 */
#define MPU60X0_SIG_COND_RESET(N)		(N)	///< Puts N into the SIG_COND_RESET field
#define MPU60X0_I2C_MST_RESET(N)		((N) << 1)	///< Puts N into the I2C_MST_RESET field
#define MPU60X0_FIFO_RESET(N)			((N) << 2)	///< Puts N into the FIFO_RESET field
#define MPU60X0_I2C_IF_DIS(N)			((N) << 4)	///< Puts N into the I2C_IF_DIS field
#define MPU60X0_I2C_MST_EN(N)			((N) << 5)	///< Puts N into the I2C_MST_EN field
#define MPU60X0_FIFO_EN(N)				((N) << 6)	///< Puts N into the FIFO_EN field

#define MPU60X0_SIG_COND_RESET_MASK		0x01	///< Mask of the SIG_COND_RESET field
#define MPU60X0_I2C_MST_RESET_MASK		0x02	///< Mask of the I2C_MST_RESET field
#define MPU60X0_FIFO_RESET_MASK			0x04	///< Mask of the FIFO_RESET field
#define MPU60X0_I2C_IF_DIS_MASK			0x10	///< Mask of the I2C_IF_DIS field
#define MPU60X0_I2C_MST_EN_MASK			0x20	///< Mask of the I2C_MST_EN field
#define MPU60X0_FIFO_EN_MASK			0x40	///< Mask of the FIFO_EN field
/** @} */


// Register : PWR_MGMT_1

/** @name MPU60X0 PWR_MGMT_1 register
 *  @{
 */
/**
 * @brief Bits 2-0 (CLKSEL): clock source.
 */
typedef enum EnPifMpu60x0Clksel
{
    MPU60X0_CLKSEL_INTERNAL_8MHZ   	= 0,
    MPU60X0_CLKSEL_PLL_XGYRO       	= 1,
    MPU60X0_CLKSEL_PLL_YGYRO       	= 2,
    MPU60X0_CLKSEL_PLL_ZGYRO       	= 3,
    MPU60X0_CLKSEL_EXTERNAL_32KHZ  	= 4,
    MPU60X0_CLKSEL_EXTERNAL_19MHZ  	= 5,
    MPU60X0_CLKSEL_KEEP_RESET      	= 7
} PifMpu60x0Clksel;

#define MPU60X0_TEMP_DIS(N)			((N) << 3)	///< Puts N into the TEMP_DIS field
#define MPU60X0_CYCLE(N)			((N) << 5)	///< Puts N into the CYCLE field
#define MPU60X0_SLEEP(N)			((N) << 6)	///< Puts N into the SLEEP field
#define MPU60X0_DEVICE_RESET(N)		((N) << 7)	///< Puts N into the DEVICE_RESET field

#define MPU60X0_CLKSEL_MASK			0x07	///< Mask of the CLKSEL field
#define MPU60X0_TEMP_DIS_MASK		0x08	///< Mask of the TEMP_DIS field
#define MPU60X0_CYCLE_MASK			0x20	///< Mask of the CYCLE field
#define MPU60X0_SLEEP_MASK			0x40	///< Mask of the SLEEP field
#define MPU60X0_DEVICE_RESET_MASK	0x80	///< Mask of the DEVICE_RESET field
/** @} */


// Register : PWR_MGMT_2

/** @name MPU60X0 PWR_MGMT_2 register
 *  @{
 */
#define MPU60X0_STBY_ZG(N)			(N)	///< Puts N into the STBY_ZG field
#define MPU60X0_STBY_YG(N)			((N) << 1)	///< Puts N into the STBY_YG field
#define MPU60X0_STBY_XG(N)			((N) << 2)	///< Puts N into the STBY_XG field
#define MPU60X0_STBY_ZA(N)			((N) << 3)	///< Puts N into the STBY_ZA field
#define MPU60X0_STBY_YA(N)			((N) << 4)	///< Puts N into the STBY_YA field
#define MPU60X0_STBY_XA(N)			((N) << 5)	///< Puts N into the STBY_XA field

/**
 * @brief Bits 7-6 (LP_WAKE_CTRL): wake-up rate in accelerometer only low power mode.
 */
typedef enum EnPifMpu60x0LpWakeCtrl
{
    MPU60X0_LP_WAKE_CTRL_1_25HZ		= 0 << 6,
    MPU60X0_LP_WAKE_CTRL_5HZ		= 1 << 6,
    MPU60X0_LP_WAKE_CTRL_20HZ		= 2 << 6,
    MPU60X0_LP_WAKE_CTRL_40HZ		= 3 << 6
} PifMpu60x0LpWakeCtrl;

#define MPU60X0_STBY_ZG_MASK		0x01	///< Mask of the STBY_ZG field
#define MPU60X0_STBY_YG_MASK		0x02	///< Mask of the STBY_YG field
#define MPU60X0_STBY_XG_MASK		0x04	///< Mask of the STBY_XG field
#define MPU60X0_STBY_ZA_MASK		0x08	///< Mask of the STBY_ZA field
#define MPU60X0_STBY_YA_MASK		0x10	///< Mask of the STBY_YA field
#define MPU60X0_STBY_XA_MASK		0x20	///< Mask of the STBY_XA field
#define MPU60X0_LP_WAKE_CTRL_MASK	0xC0	///< Mask of the LP_WAKE_CTRL field
/** @} */


/**
 * @class StPifMpu60x0
 * @brief MPU-6000/6050 gyroscope and accelerometer.
 */
typedef struct StPifMpu60x0
{
	// Public Member Variable
	uint8_t gyro_scale;	///< Divider of the raw gyro values, 0 for none
	uint8_t accel_scale;	///< Divider of the raw accelerometer values, 0 for none
	uint8_t temp_scale;	///< Multiplier of the temperature

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;	///< I2C or SPI device of the sensor


	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifMpu60x0;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads the full-scale ranges and registers the sensor on the IMU sensor. Called by the
 *        bus specific Init functions once _p_device is set up and the chip has been reset.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_Config(PifMpu60x0* p_owner, PifId id, PifImuSensor* p_imu_sensor);

/**
 * @brief Writes the GYRO_CONFIG register and updates the gyro gain of the IMU sensor to its range.
 * @param p_owner Pointer to the owner instance.
 * @param gyro_config Fields of the register, ORed together.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_SetGyroConfig(PifMpu60x0* p_owner, uint8_t gyro_config);

/**
 * @brief Sets the gyro range and updates the gyro gain of the IMU sensor to it.
 * @param p_owner Pointer to the owner instance.
 * @param fs_sel Gyroscope full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_SetFsSel(PifMpu60x0* p_owner, PifMpu60x0FsSel fs_sel);

/**
 * @brief Writes the ACCEL_CONFIG register and updates the accelerometer gain of the IMU sensor to its range.
 * @param p_owner Pointer to the owner instance.
 * @param accel_config Fields of the register, ORed together.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_SetAccelConfig(PifMpu60x0* p_owner, uint8_t accel_config);

/**
 * @brief Sets the accelerometer range and updates the accelerometer gain of the IMU sensor to it.
 * @param p_owner Pointer to the owner instance.
 * @param afs_sel Accelerometer full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_SetAfsSel(PifMpu60x0* p_owner, PifMpu60x0AfsSel afs_sel);

/**
 * @brief Reads the raw gyro values of the 3 axes, divided by gyro_scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_ReadGyro(PifMpu60x0* p_owner, int16_t* p_gyro);

/**
 * @brief Reads the raw accelerometer values of the 3 axes, divided by accel_scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_ReadAccel(PifMpu60x0* p_owner, int16_t* p_accel);

/**
 * @brief Reads the temperature in degrees C, multiplied by temp_scale.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Receives the temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_ReadTemperature(PifMpu60x0* p_owner, int16_t* p_temperature);

/**
 * @brief Measures the gyro bias and noise at rest, taking a sample every 5 ms.
 *        Wraps pifImuSensor_CalibrateGyro().
 * @param p_owner Pointer to the owner instance.
 * @param samples Number of samples to average.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_CalibrationGyro(PifMpu60x0* p_owner, uint8_t samples);

/**
 * @brief Sets the gyro deadband as a multiple of the measured noise, calibrating
 *        with 50 samples first if needed. Wraps pifImuSensor_SetGyroDeadband().
 * @param p_owner Pointer to the owner instance.
 * @param multiple Deadband width in noise standard deviations. 0 turns it off.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0_SetThreshold(PifMpu60x0* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU60X0_H
