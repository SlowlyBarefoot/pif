// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_mpu6500.h
 * @brief InvenSense MPU-6500 6-axis gyroscope and accelerometer, as a PifImuSensor.
 *
 * pif_mpu6500_i2c and pif_mpu6500_spi attach the device. The register map below follows the register map
 * document of the chip.
 */
#ifndef PIF_MPU6500_H
#define PIF_MPU6500_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "sensor/pif_imu_sensor.h"


#define MPU6500_I2C_ADDR(N)			(0x68 + (N))	///< I2C address with AD0 on GND (0) or VDD (1)

#define MPU6500_WHO_AM_I_CONST		0x70	///< Value of the WHO_AM_I register


/**
 * @brief Register addresses, named as in the register map document.
 */
typedef enum EnPifMpu6500Reg
{
	MPU6500_REG_SELF_TEST_X_GYRO  	= 0x00,
	MPU6500_REG_SELF_TEST_Y_GYRO  	= 0x01,
	MPU6500_REG_SELF_TEST_Z_GYRO  	= 0x02,
	MPU6050_REG_XA_OFFSET_H        	= 0x06,
	MPU6050_REG_XA_OFFSET_L        	= 0x07,
	MPU6050_REG_YA_OFFSET_H        	= 0x08,
	MPU6050_REG_YA_OFFSET_L        	= 0x09,
	MPU6050_REG_ZA_OFFSET_H        	= 0x0A,
	MPU6050_REG_ZA_OFFSET_L        	= 0x0B,
	MPU6500_REG_SELF_TEST_X_ACCEL  	= 0x0D,
	MPU6500_REG_SELF_TEST_Y_ACCEL  	= 0x0E,
	MPU6500_REG_SELF_TEST_Z_ACCEL  	= 0x0F,
	MPU6500_REG_XG_OFFSET_H      	= 0x13,
	MPU6500_REG_XG_OFFSET_L      	= 0x14,
	MPU6500_REG_YG_OFFSET_H      	= 0x15,
	MPU6500_REG_YG_OFFSET_L      	= 0x16,
	MPU6500_REG_ZG_OFFSET_H      	= 0x17,
	MPU6500_REG_ZG_OFFSET_L      	= 0x18,
	MPU6500_REG_SMPLRT_DIV      	= 0x19,
	MPU6500_REG_CONFIG         	  	= 0x1A,
	MPU6500_REG_GYRO_CONFIG     	= 0x1B,
	MPU6500_REG_ACCEL_CONFIG    	= 0x1C,
	MPU6500_REG_ACCEL_CONFIG2    	= 0x1D,
	MPU6500_REG_LP_ACCEL_ODR    	= 0x1E,
	MPU6500_REG_WOM_THR		    	= 0x1F,
	MPU6500_REG_FIFO_EN        	  	= 0x23,
	MPU6500_REG_I2C_MST_CTRL   	  	= 0x24,
	MPU6500_REG_I2C_SLV0_ADDR  	  	= 0x25,
	MPU6500_REG_I2C_SLV0_REG   	  	= 0x26,
	MPU6500_REG_I2C_SLV0_CTRL    	= 0x27,
	MPU6500_REG_I2C_SLV1_ADDR   	= 0x28,
	MPU6500_REG_I2C_SLV1_REG    	= 0x29,
	MPU6500_REG_I2C_SLV1_CTRL  	  	= 0x2A,
	MPU6500_REG_I2C_SLV2_ADDR  	  	= 0x2B,
	MPU6500_REG_I2C_SLV2_REG   	  	= 0x2C,
	MPU6500_REG_I2C_SLV2_CTRL  	  	= 0x2D,
	MPU6500_REG_I2C_SLV3_ADDR  	  	= 0x2E,
	MPU6500_REG_I2C_SLV3_REG    	= 0x2F,
	MPU6500_REG_I2C_SLV3_CTRL   	= 0x30,
	MPU6500_REG_I2C_SLV4_ADDR   	= 0x31,
	MPU6500_REG_I2C_SLV4_REG    	= 0x32,
	MPU6500_REG_I2C_SLV4_DO     	= 0x33,
	MPU6500_REG_I2C_SLV4_CTRL   	= 0x34,
	MPU6500_REG_I2C_SLV4_DI     	= 0x35,
	MPU6500_REG_I2C_MST_STATUS  	= 0x36,
	MPU6500_REG_INT_PIN_CFG      	= 0x37,
	MPU6500_REG_INT_ENABLE       	= 0x38,
	MPU6500_REG_INT_STATUS       	= 0x3A,
	MPU6500_REG_ACCEL_XOUT_H    	= 0x3B,
	MPU6500_REG_ACCEL_XOUT_L     	= 0x3C,
	MPU6500_REG_ACCEL_YOUT_H     	= 0x3D,
	MPU6500_REG_ACCEL_YOUT_L     	= 0x3E,
	MPU6500_REG_ACCEL_ZOUT_H    	= 0x3F,
	MPU6500_REG_ACCEL_ZOUT_L    	= 0x40,
	MPU6500_REG_TEMP_OUT_H      	= 0x41,
	MPU6500_REG_TEMP_OUT_L      	= 0x42,
	MPU6500_REG_GYRO_XOUT_H     	= 0x43,
	MPU6500_REG_GYRO_XOUT_L     	= 0x44,
	MPU6500_REG_GYRO_YOUT_H     	= 0x45,
	MPU6500_REG_GYRO_YOUT_L      	= 0x46,
	MPU6500_REG_GYRO_ZOUT_H     	= 0x47,
	MPU6500_REG_GYRO_ZOUT_L     	= 0x48,
	MPU6500_REG_EXT_SENS_DATA_00	= 0x49,
	MPU6500_REG_EXT_SENS_DATA_01	= 0x4A,
	MPU6500_REG_EXT_SENS_DATA_02	= 0x4B,
	MPU6500_REG_EXT_SENS_DATA_03	= 0x4C,
	MPU6500_REG_EXT_SENS_DATA_04	= 0x4D,
	MPU6500_REG_EXT_SENS_DATA_05	= 0x4E,
	MPU6500_REG_EXT_SENS_DATA_06	= 0x4F,
	MPU6500_REG_EXT_SENS_DATA_07	= 0x50,
	MPU6500_REG_EXT_SENS_DATA_08	= 0x51,
	MPU6500_REG_EXT_SENS_DATA_09	= 0x52,
	MPU6500_REG_EXT_SENS_DATA_10	= 0x53,
	MPU6500_REG_EXT_SENS_DATA_11	= 0x54,
	MPU6500_REG_EXT_SENS_DATA_12	= 0x55,
	MPU6500_REG_EXT_SENS_DATA_13	= 0x56,
	MPU6500_REG_EXT_SENS_DATA_14	= 0x57,
	MPU6500_REG_EXT_SENS_DATA_15	= 0x58,
	MPU6500_REG_EXT_SENS_DATA_16	= 0x59,
	MPU6500_REG_EXT_SENS_DATA_17	= 0x5A,
	MPU6500_REG_EXT_SENS_DATA_18	= 0x5B,
	MPU6500_REG_EXT_SENS_DATA_19	= 0x5C,
	MPU6500_REG_EXT_SENS_DATA_20	= 0x5D,
	MPU6500_REG_EXT_SENS_DATA_21	= 0x5E,
	MPU6500_REG_EXT_SENS_DATA_22	= 0x5F,
	MPU6500_REG_EXT_SENS_DATA_23	= 0x60,
	MPU6500_REG_I2C_SLV0_DO     	= 0x63,
	MPU6500_REG_I2C_SLV1_DO     	= 0x64,
	MPU6500_REG_I2C_SLV2_DO     	= 0x65,
	MPU6500_REG_I2C_SLV3_DO     	= 0x66,
	MPU6500_REG_I2C_MST_DELAY_CTRL	= 0x67,
	MPU6500_REG_SIGNAL_PATH_RESET  	= 0x68,
	MPU6500_REG_ACCEL_INTEL_CTRL  	= 0x69,
	MPU6500_REG_USER_CTRL       	= 0x6A,
	MPU6500_REG_PWR_MGMT_1      	= 0x6B,
	MPU6500_REG_PWR_MGMT_2      	= 0x6C,
	MPU6500_REG_FIFO_COUNT_H     	= 0x72,
	MPU6500_REG_FIFO_COUNT_L     	= 0x73,
	MPU6500_REG_FIFO_R_W        	= 0x74,
	MPU6500_REG_WHO_AM_I        	= 0x75,
	MPU6500_REG_XA_OFFSET_H        	= 0x77,
	MPU6500_REG_XA_OFFSET_L        	= 0x78,
	MPU6500_REG_YA_OFFSET_H        	= 0x7A,
	MPU6500_REG_YA_OFFSET_L        	= 0x7B,
	MPU6500_REG_ZA_OFFSET_H        	= 0x7D,
	MPU6500_REG_ZA_OFFSET_L        	= 0x7E
} PifMpu6500Reg;


// Register : CONFIG

/** @name MPU6500 CONFIG register
 *  @{
 */
/**
 * @brief Bits 2-0 (DLPF_CFG): bandwidth of the digital low pass filter of the gyro and the temperature.
 */
typedef enum EnPifMpu6500DlpfCfg
{
	MPU6500_DLPF_CFG_250HZ				= 0,
	MPU6500_DLPF_CFG_184HZ				= 1,
	MPU6500_DLPF_CFG_92HZ				= 2,
	MPU6500_DLPF_CFG_41HZ				= 3,
	MPU6500_DLPF_CFG_20HZ				= 4,
	MPU6500_DLPF_CFG_10HZ				= 5,
    MPU6500_DLPF_CFG_5HZ				= 6,
	MPU6500_DLPF_CFG_3600HZ				= 7
} PifMpu6500DlpfCfg;

/**
 * @brief Bits 5-3 (EXT_SYNC_SET): sample whose LSB carries the FSYNC input.
 */
typedef enum EnPifMpu6500ExtSyncSet
{
	MPU6500_EXT_SYNC_SET_DISABLE		= 0 << 3,
	MPU6500_EXT_SYNC_SET_TEMP_OUT_L		= 1 << 3,
	MPU6500_EXT_SYNC_SET_GYRO_XOUT_L	= 2 << 3,
	MPU6500_EXT_SYNC_SET_GYRO_YOUT_L	= 3 << 3,
	MPU6500_EXT_SYNC_SET_GYRO_ZOUT_L	= 4 << 3,
	MPU6500_EXT_SYNC_SET_ACCEL_XOUT_L	= 5 << 3,
    MPU6500_EXT_SYNC_SET_ACCEL_YOUT_L	= 6 << 3,
    MPU6500_EXT_SYNC_SET_ACCEL_ZOUT_L	= 7 << 3
} PifMpu6500ExtSyncSet;

#define MPU6500_FIFO_MODE(N)			((N) << 6)	///< Puts N into the FIFO_MODE field

#define MPU6500_DLPF_CFG_MASK			0x07	///< Mask of the DLPF_CFG field
#define MPU6500_EXT_SYNC_SET_MASK		0x38	///< Mask of the EXT_SYNC_SET field
#define MPU6500_FIFO_MODE_MASK			0x40	///< Mask of the FIFO_MODE field
/** @} */


// Register : GYRO_CONFIG

/** @name MPU6500 GYRO_CONFIG register
 *  @{
 */
/**
 * @brief Bits 1-0 (FCHOICE_B): bypass of the digital low pass filter of the gyro.
 */
typedef enum EnPifMpu6500FchoiceB
{
    MPU6500_FCHOICE_B_DISABLE		= 0,
    MPU6500_FCHOICE_B_8800HZ		= 1,
    MPU6500_FCHOICE_B_3600HZ		= 2
} PifMpu6500FchoiceB;

/**
 * @brief Bits 4-3 (GYRO_FS_SEL): full scale range of the gyro.
 */
typedef enum EnPifMpu6500GyroFsSel
{
    MPU6500_GYRO_FS_SEL_250DPS		= 0 << 3,
    MPU6500_GYRO_FS_SEL_500DPS		= 1 << 3,
    MPU6500_GYRO_FS_SEL_1000DPS		= 2 << 3,
    MPU6500_GYRO_FS_SEL_2000DPS		= 3 << 3
} PifMpu6500GyroFsSel;

#define MPU6500_ZG_ST(N)			((N) << 5)	///< Puts N into the ZG_ST field
#define MPU6500_YG_ST(N)			((N) << 6)	///< Puts N into the YG_ST field
#define MPU6500_XG_ST(N)			((N) << 7)	///< Puts N into the XG_ST field

#define MPU6500_FCHOICE_B_MASK		0x03		///< Mask of the FCHOICE_B field
#define MPU6500_GYRO_FS_SEL_MASK	0x18		///< Use pifMpu6500_SetGyroConfig or pifMpu6500_SetGyroFsSel to change this value.
#define MPU6500_ZG_ST_MASK			0x20		///< Mask of the ZG_ST field
#define MPU6500_YG_ST_MASK			0x40		///< Mask of the YG_ST field
#define MPU6500_XG_ST_MASK			0x80		///< Mask of the XG_ST field
/** @} */


// Register : ACCEL_CONFIG

/** @name MPU6500 ACCEL_CONFIG register
 *  @{
 */
/**
 * @brief Bits 4-3 (ACCEL_FS_SEL): full scale range of the accelerometer.
 */
typedef enum EnPifMpu6500AccelFsSel
{
    MPU6500_ACCEL_FS_SEL_2G			= 0 << 3,
    MPU6500_ACCEL_FS_SEL_4G			= 1 << 3,
    MPU6500_ACCEL_FS_SEL_8G			= 2 << 3,
    MPU6500_ACCEL_FS_SEL_16G		= 3 << 3
} PifMpu6500AccelFsSel;

#define MPU6500_ZA_ST(N)			((N) << 5)	///< Puts N into the ZA_ST field
#define MPU6500_YA_ST(N)			((N) << 6)	///< Puts N into the YA_ST field
#define MPU6500_XA_ST(N)			((N) << 7)	///< Puts N into the XA_ST field

#define MPU6500_ACCEL_FS_SEL_MASK	0x18		///< Use pifMpu6500_SetAccelConfig or pifMpu6500_SetAfsSel to change this value.
#define MPU6500_ZA_ST_MASK			0x20		///< Mask of the ZA_ST field
#define MPU6500_YA_ST_MASK			0x40		///< Mask of the YA_ST field
#define MPU6500_XA_ST_MASK			0x80		///< Mask of the XA_ST field
/** @} */


// Register : ACCEL_CONFIG_2

/** @name MPU6500 ACCEL_CONFIG_2 register
 *  @{
 */
/**
 * @brief Bits 2-0 (A_DLPF_CFG): bandwidth of the digital low pass filter of the accelerometer.
 */
typedef enum EnPifMpu6500ADlpfCfg
{
    MPU6500_A_DLPF_CFG_460HZ			= 0,
    MPU6500_A_DLPF_CFG_184HZ			= 1,
    MPU6500_A_DLPF_CFG_92HZ				= 2,
    MPU6500_A_DLPF_CFG_41HZ				= 3,
    MPU6500_A_DLPF_CFG_20HZ				= 4,
    MPU6500_A_DLPF_CFG_10HZ				= 5,
    MPU6500_A_DLPF_CFG_5HZ				= 6
} PifMpu6500ADlpfCfg;

#define MPU6500_ACCEL_FCHOICE_B(N)		((N) << 3)	///< Puts N into the ACCEL_FCHOICE_B field

#define MPU6500_A_DLPF_CFG_MASK			0x07	///< Mask of the A_DLPF_CFG field
#define MPU6500_ACCEL_FCHOICE_B_MASK	0x08	///< Mask of the ACCEL_FCHOICE_B field
/** @} */


// Register : LP_ACCEL_ODR

/** @name MPU6500 LP_ACCEL_ODR register
 *  @{
 */
/**
 * @brief Bits 3-0 (LPOSC_CLKSEL): output data rate of the accelerometer in low power mode.
 */
typedef enum EnPifMpu6500LposcClksel
{
    MPU6500_LPOSC_CLKSEL_0_24		= 0,
    MPU6500_LPOSC_CLKSEL_0_49		= 1,
    MPU6500_LPOSC_CLKSEL_0_98		= 2,
    MPU6500_LPOSC_CLKSEL_1_95		= 3,
    MPU6500_LPOSC_CLKSEL_3_91		= 4,
    MPU6500_LPOSC_CLKSEL_7_81		= 5,
    MPU6500_LPOSC_CLKSEL_15_63		= 6,
    MPU6500_LPOSC_CLKSEL_31_25		= 7,
    MPU6500_LPOSC_CLKSEL_62_50		= 8,
    MPU6500_LPOSC_CLKSEL_125		= 9,
    MPU6500_LPOSC_CLKSEL_250		= 10,
    MPU6500_LPOSC_CLKSEL_500		= 11
} PifMpu6500LposcClksel;

#define MPU6500_LPOSC_CLKSEL_MASK	0x0F	///< Mask of the LPOSC_CLKSEL field
/** @} */


// Register : FIFO_EN

/** @name MPU6500 FIFO_EN register
 *  @{
 */
#define MPU6500_SLV_0(N)		(N)	///< Puts N into the SLV_0 field
#define MPU6500_SLV_1(N)		((N) << 1)	///< Puts N into the SLV_1 field
#define MPU6500_SLV_2(N)		((N) << 2)	///< Puts N into the SLV_2 field
#define MPU6500_ACCEL(N)		((N) << 3)	///< Puts N into the ACCEL field
#define MPU6500_GYRO_ZOUT(N)	((N) << 4)	///< Puts N into the GYRO_ZOUT field
#define MPU6500_GYRO_YOUT(N)	((N) << 5)	///< Puts N into the GYRO_YOUT field
#define MPU6500_GYRO_XOUT(N)	((N) << 6)	///< Puts N into the GYRO_XOUT field
#define MPU6500_TEMP_OUT(N)		((N) << 7)	///< Puts N into the TEMP_OUT field

#define MPU6500_SLV_0_MASK		0x01	///< Mask of the SLV_0 field
#define MPU6500_SLV_1_MASK		0x02	///< Mask of the SLV_1 field
#define MPU6500_SLV_2_MASK		0x04	///< Mask of the SLV_2 field
#define MPU6500_ACCEL_MASK		0x08	///< Mask of the ACCEL field
#define MPU6500_GYRO_ZOUT_MASK	0x10	///< Mask of the GYRO_ZOUT field
#define MPU6500_GYRO_YOUT_MASK	0x20	///< Mask of the GYRO_YOUT field
#define MPU6500_GYRO_XOUT_MASK	0x40	///< Mask of the GYRO_XOUT field
#define MPU6500_TEMP_OUT_MASK	0x80	///< Mask of the TEMP_OUT field
/** @} */


// Register : I2C_MST_CTRL

/** @name MPU6500 I2C_MST_CTRL register
 *  @{
 */
/**
 * @brief Bits 3-0 (I2C_MST_CLK): clock of the auxiliary I2C master.
 */
typedef enum EnPifMpu6500I2cMstClk
{
    MPU6500_I2C_MST_CLK_348KHZ		= 0,
    MPU6500_I2C_MST_CLK_333KHZ		= 1,
    MPU6500_I2C_MST_CLK_320KHZ		= 2,
    MPU6500_I2C_MST_CLK_308KHZ		= 3,
    MPU6500_I2C_MST_CLK_296KHZ		= 4,
    MPU6500_I2C_MST_CLK_286KHZ		= 5,
    MPU6500_I2C_MST_CLK_276KHZ		= 6,
    MPU6500_I2C_MST_CLK_267KHZ		= 7,
    MPU6500_I2C_MST_CLK_258KHZ		= 8,
    MPU6500_I2C_MST_CLK_500KHZ		= 9,
    MPU6500_I2C_MST_CLK_471KHZ		= 10,
    MPU6500_I2C_MST_CLK_444KHZ		= 11,
    MPU6500_I2C_MST_CLK_421KHZ		= 12,
    MPU6500_I2C_MST_CLK_400KHZ		= 13,
    MPU6500_I2C_MST_CLK_381KHZ		= 14,
    MPU6500_I2C_MST_CLK_364KHZ		= 15
} PifMpu6500I2cMstClk;

#define MPU6500_I2C_MST_P_NSR(N)	((N) << 4)	///< Puts N into the I2C_MST_P_NSR field
#define MPU6500_SLV_3_FIFO_EN(N)	((N) << 5)	///< Puts N into the SLV_3_FIFO_EN field
#define MPU6500_WAIT_FOR_ES(N)		((N) << 6)	///< Puts N into the WAIT_FOR_ES field
#define MPU6500_MULT_MST_EN(N)		((N) << 7)	///< Puts N into the MULT_MST_EN field

#define MPU6500_I2C_MST_CLK_MASK	0x0F	///< Mask of the I2C_MST_CLK field
#define MPU6500_I2C_MST_P_NSR_MASK	0x10	///< Mask of the I2C_MST_P_NSR field
#define MPU6500_SLV_3_FIFO_EN_MASK	0x20	///< Mask of the SLV_3_FIFO_EN field
#define MPU6500_WAIT_FOR_ES_MASK	0x40	///< Mask of the WAIT_FOR_ES field
#define MPU6500_MULT_MST_EN_MASK	0x80	///< Mask of the MULT_MST_EN field
/** @} */


// Register : I2C_SLVx_ADDR		x = 0 ~ 4

/** @name MPU6500 I2C_SLV0_ADDR to I2C_SLV4_ADDR registers
 *  @{
 */
#define MPU6500_I2C_ID_x(N)			(N)	///< Puts N into the I2C_ID_x field
#define MPU6500_I2C_SLVx_RNW(N)		((N) << 7)	///< Puts N into the I2C_SLVx_RNW field

#define MPU6500_I2C_ID_x_MASK		0x7F	///< Mask of the I2C_ID_x field
#define MPU6500_I2C_SLVx_RNW_MASK	0x80	///< Mask of the I2C_SLVx_RNW field
/** @} */


// Register : I2C_SLVx_CTRL		x = 0 ~ 3

/** @name MPU6500 I2C_SLV0_CTRL to I2C_SLV3_CTRL registers
 *  @{
 */
#define MPU6500_I2C_SLVx_LENG(N)		(N)	///< Puts N into the I2C_SLVx_LENG field
#define MPU6500_I2C_SLVx_GRP(N)			((N) << 4)	///< Puts N into the I2C_SLVx_GRP field
#define MPU6500_I2C_SLVx_REG_DIS(N)		((N) << 5)	///< Puts N into the I2C_SLVx_REG_DIS field
#define MPU6500_I2C_SLVx_BYTE_SW(N)		((N) << 6)	///< Puts N into the I2C_SLVx_BYTE_SW field
#define MPU6500_I2C_SLVx_EN(N)			((N) << 7)	///< Puts N into the I2C_SLVx_EN field

#define MPU6500_I2C_SLVx_LENG_MASK		0x0F	///< Mask of the I2C_SLVx_LENG field
#define MPU6500_I2C_SLVx_GRP_MASK		0x10	///< Mask of the I2C_SLVx_GRP field
#define MPU6500_I2C_SLVx_REG_DIS_MASK	0x20	///< Mask of the I2C_SLVx_REG_DIS field
#define MPU6500_I2C_SLVx_BYTE_SW_MASK	0x40	///< Mask of the I2C_SLVx_BYTE_SW field
#define MPU6500_I2C_SLVx_EN_MASK		0x80	///< Mask of the I2C_SLVx_EN field
/** @} */


// Register : I2C_SLV4_CTRL

/** @name MPU6500 I2C_SLV4_CTRL register
 *  @{
 */
#define MPU6500_I2C_MST_DLY(N)			(N)	///< Puts N into the I2C_MST_DLY field
#define MPU6500_I2C_SLV4_REG_DIS(N)		((N) << 5)	///< Puts N into the I2C_SLV4_REG_DIS field
#define MPU6500_SLV4_DONE_INT_EN(N)		((N) << 6)	///< Puts N into the SLV4_DONE_INT_EN field
#define MPU6500_I2C_SLV4_EN(N)			((N) << 7)	///< Puts N into the I2C_SLV4_EN field

#define MPU6500_I2C_MST_DLY_MASK		0x1F	///< Mask of the I2C_MST_DLY field
#define MPU6500_I2C_SLV4_REG_DIS_MASK	0x20	///< Mask of the I2C_SLV4_REG_DIS field
#define MPU6500_SLV4_DONE_INT_EN_MASK	0x40	///< Mask of the SLV4_DONE_INT_EN field
#define MPU6500_I2C_SLV4_EN_MASK		0x80	///< Mask of the I2C_SLV4_EN field
/** @} */


// Register : I2C_MST_STSTUS

/** @name MPU6500 I2C_MST_STATUS register
 *  @{
 */
#define MPU6500_I2C_SLV0_NACK(N)	(N)	///< Puts N into the I2C_SLV0_NACK field
#define MPU6500_I2C_SLV1_NACK(N)	((N) << 1)	///< Puts N into the I2C_SLV1_NACK field
#define MPU6500_I2C_SLV2_NACK(N)	((N) << 2)	///< Puts N into the I2C_SLV2_NACK field
#define MPU6500_I2C_SLV3_NACK(N)	((N) << 3)	///< Puts N into the I2C_SLV3_NACK field
#define MPU6500_I2C_SLV4_NACK(N)	((N) << 4)	///< Puts N into the I2C_SLV4_NACK field
#define MPU6500_I2C_LOST_ARB(N)		((N) << 5)	///< Puts N into the I2C_LOST_ARB field
#define MPU6500_I2C_SLV4_DONE(N)	((N) << 6)	///< Puts N into the I2C_SLV4_DONE field
#define MPU6500_PASS_THROUGH(N)		((N) << 7)	///< Puts N into the PASS_THROUGH field

#define MPU6500_I2C_SLV0_NACK_MASK	0x01	///< Mask of the I2C_SLV0_NACK field
#define MPU6500_I2C_SLV1_NACK_MASK	0x02	///< Mask of the I2C_SLV1_NACK field
#define MPU6500_I2C_SLV2_NACK_MASK	0x04	///< Mask of the I2C_SLV2_NACK field
#define MPU6500_I2C_SLV3_NACK_MASK	0x08	///< Mask of the I2C_SLV3_NACK field
#define MPU6500_I2C_SLV4_NACK_MASK	0x10	///< Mask of the I2C_SLV4_NACK field
#define MPU6500_I2C_LOST_ARB_MASK	0x20	///< Mask of the I2C_LOST_ARB field
#define MPU6500_I2C_SLV4_DONE_MASK	0x40	///< Mask of the I2C_SLV4_DONE field
#define MPU6500_PASS_THROUGH_MASK	0x80	///< Mask of the PASS_THROUGH field
/** @} */


// Register : INT_PIN_CFG

/** @name MPU6500 INT_PIN_CFG register
 *  @{
 */
#define MPU6500_BYPASS_EN(N)			((N) << 1)	///< Puts N into the BYPASS_EN field
#define MPU6500_FSYNC_INT_MODE_EN(N)	((N) << 2)	///< Puts N into the FSYNC_INT_MODE_EN field
#define MPU6500_ACTL_FSYNC(N)			((N) << 3)	///< Puts N into the ACTL_FSYNC field
#define MPU6500_INT_ANYRD_2CLEAR(N)		((N) << 4)	///< Puts N into the INT_ANYRD_2CLEAR field
#define MPU6500_LATCH_INT_EN(N)			((N) << 5)	///< Puts N into the LATCH_INT_EN field
#define MPU6500_OPEN(N)					((N) << 6)	///< Puts N into the OPEN field
#define MPU6500_ACTL(N)					((N) << 7)	///< Puts N into the ACTL field

#define MPU6500_BYPASS_EN_MASK			0x02	///< Mask of the BYPASS_EN field
#define MPU6500_FSYNC_INT_MODE_EN_MASK	0x04	///< Mask of the FSYNC_INT_MODE_EN field
#define MPU6500_ACTL_FSYNC_MASK			0x08	///< Mask of the ACTL_FSYNC field
#define MPU6500_INT_ANYRD_2CLEAR_MASK	0x10	///< Mask of the INT_ANYRD_2CLEAR field
#define MPU6500_LATCH_INT_EN_MASK		0x20	///< Mask of the LATCH_INT_EN field
#define MPU6500_OPEN_MASK				0x40	///< Mask of the OPEN field
#define MPU6500_ACTL_MASK				0x80	///< Mask of the ACTL field
/** @} */


// Register : INT_ENABLE

/** @name MPU6500 INT_ENABLE register
 *  @{
 */
#define MPU6500_RAW_RDY_EN(N)			(N)	///< Puts N into the RAW_RDY_EN field
#define MPU6500_FSYNC_INT_EN(N)			((N) << 3)	///< Puts N into the FSYNC_INT_EN field
#define MPU6500_FIFO_OVERFLOW_EN(N)		((N) << 4)	///< Puts N into the FIFO_OVERFLOW_EN field
#define MPU6500_WOM_EN(N)				((N) << 6)	///< Puts N into the WOM_EN field

#define MPU6500_RAW_RDY_EN_MASK			0x01	///< Mask of the RAW_RDY_EN field
#define MPU6500_FSYNC_INT_EN_MASK		0x08	///< Mask of the FSYNC_INT_EN field
#define MPU6500_FIFO_OVERFLOW_EN_MASK	0x10	///< Mask of the FIFO_OVERFLOW_EN field
#define MPU6500_WOM_EN_MASK				0x40	///< Mask of the WOM_EN field
/** @} */


// Register : INT_STATUS

/** @name MPU6500 INT_STATUS register
 *  @{
 */
#define MPU6500_RAW_RDY_INT(N)			(N)	///< Puts N into the RAW_RDY_INT field
#define MPU6500_DMP_INT(N)				((N) << 1)	///< Puts N into the DMP_INT field
#define MPU6500_FSYNC_INT(N)			((N) << 3)	///< Puts N into the FSYNC_INT field
#define MPU6500_FIFO_OVERFLOW_INT(N)	((N) << 4)	///< Puts N into the FIFO_OVERFLOW_INT field
#define MPU6500_WOM_INT(N)				((N) << 6)	///< Puts N into the WOM_INT field

#define MPU6500_RAW_RDY_INT_MASK		0x01	///< Mask of the RAW_RDY_INT field
#define MPU6500_DMP_INT_MASK			0x02	///< Mask of the DMP_INT field
#define MPU6500_FSYNC_INT_MASK			0x08	///< Mask of the FSYNC_INT field
#define MPU6500_FIFO_OVERFLOW_INT_MASK	0x10	///< Mask of the FIFO_OVERFLOW_INT field
#define MPU6500_WOM_INT_MASK			0x40	///< Mask of the WOM_INT field
/** @} */


// Register : I2C_MST_DELAY_CTRL

/** @name MPU6500 I2C_MST_DELAY_CTRL register
 *  @{
 */
#define MPU6500_I2C_SLV0_DLY_EN(N)		(N)	///< Puts N into the I2C_SLV0_DLY_EN field
#define MPU6500_I2C_SLV1_DLY_EN(N)		((N) << 1)	///< Puts N into the I2C_SLV1_DLY_EN field
#define MPU6500_I2C_SLV2_DLY_EN(N)		((N) << 2)	///< Puts N into the I2C_SLV2_DLY_EN field
#define MPU6500_I2C_SLV3_DLY_EN(N)		((N) << 3)	///< Puts N into the I2C_SLV3_DLY_EN field
#define MPU6500_I2C_SLV4_DLY_EN(N)		((N) << 4)	///< Puts N into the I2C_SLV4_DLY_EN field
#define MPU6500_DELAY_ES_SHADOW(N)		((N) << 7)	///< Puts N into the DELAY_ES_SHADOW field

#define MPU6500_I2C_SLV0_DLY_EN_MASK	0x01	///< Mask of the I2C_SLV0_DLY_EN field
#define MPU6500_I2C_SLV1_DLY_EN_MASK	0x02	///< Mask of the I2C_SLV1_DLY_EN field
#define MPU6500_I2C_SLV2_DLY_EN_MASK	0x04	///< Mask of the I2C_SLV2_DLY_EN field
#define MPU6500_I2C_SLV3_DLY_EN_MASK	0x08	///< Mask of the I2C_SLV3_DLY_EN field
#define MPU6500_I2C_SLV4_DLY_EN_MASK	0x10	///< Mask of the I2C_SLV4_DLY_EN field
#define MPU6500_DELAY_ES_SHADOW_MASK	0x80	///< Mask of the DELAY_ES_SHADOW field
/** @} */


// Register : SIGNAL_PATH_RESET

/** @name MPU6500 SIGNAL_PATH_RESET register
 *  @{
 */
#define MPU6500_TEMP_RESET(N)		(N)	///< Puts N into the TEMP_RESET field
#define MPU6500_ACCEL_RESET(N)		((N) << 1)	///< Puts N into the ACCEL_RESET field
#define MPU6500_GYRO_RESET(N)		((N) << 2)	///< Puts N into the GYRO_RESET field

#define MPU6500_TEMP_RESET_MASK		0x01	///< Mask of the TEMP_RESET field
#define MPU6500_ACCEL_RESET_MASK	0x02	///< Mask of the ACCEL_RESET field
#define MPU6500_GYRO_RESET_MASK		0x04	///< Mask of the GYRO_RESET field
/** @} */


// Register : ACCEL_INTEL_CTRL

/** @name MPU6500 ACCEL_INTEL_CTRL register
 *  @{
 */
#define MPU6500_ACCEL_INTEL_MODE(N)		((N) << 6)	///< Puts N into the ACCEL_INTEL_MODE field
#define MPU6500_ACCEL_INTEL_EN(N)		((N) << 7)	///< Puts N into the ACCEL_INTEL_EN field

#define MPU6500_ACCEL_INTEL_MODE_MASK	0x40	///< Mask of the ACCEL_INTEL_MODE field
#define MPU6500_ACCEL_INTEL_EN_MASK		0x80	///< Mask of the ACCEL_INTEL_EN field
/** @} */


// Register : USER_CTRL

/** @name MPU6500 USER_CTRL register
 *  @{
 */
#define MPU6500_SIG_COND_RST(N)		(N)	///< Puts N into the SIG_COND_RST field
#define MPU6500_I2C_MST_RST(N)		((N) << 1)	///< Puts N into the I2C_MST_RST field
#define MPU6500_FIFO_RST(N)			((N) << 2)	///< Puts N into the FIFO_RST field
#define MPU6500_DMP_RST(N)			((N) << 3)	///< Puts N into the DMP_RST field
#define MPU6500_I2C_IF_DIS(N)		((N) << 4)	///< Puts N into the I2C_IF_DIS field
#define MPU6500_I2C_MST_EN(N)		((N) << 5)	///< Puts N into the I2C_MST_EN field
#define MPU6500_FIFO_EN(N)			((N) << 6)	///< Puts N into the FIFO_EN field
#define MPU6500_DMP_EN(N)			((N) << 7)	///< Puts N into the DMP_EN field

#define MPU6500_SIG_COND_RST_MASK	0x01	///< Mask of the SIG_COND_RST field
#define MPU6500_I2C_MST_RST_MASK	0x02	///< Mask of the I2C_MST_RST field
#define MPU6500_FIFO_RST_MASK		0x04	///< Mask of the FIFO_RST field
#define MPU6500_DMP_RST_MASK		0x08	///< Mask of the DMP_RST field
#define MPU6500_I2C_IF_DIS_MASK		0x10	///< Mask of the I2C_IF_DIS field
#define MPU6500_I2C_MST_EN_MASK		0x20	///< Mask of the I2C_MST_EN field
#define MPU6500_FIFO_EN_MASK		0x40	///< Mask of the FIFO_EN field
#define MPU6500_DMP_EN_MASK			0x80	///< Mask of the DMP_EN field
/** @} */


// Register : PWR_MGMT_1

/** @name MPU6500 PWR_MGMT_1 register
 *  @{
 */
/**
 * @brief Bits 2-0 (CLKSEL): clock source.
 */
typedef enum EnPifMpu6500Clksel
{
    MPU6500_CLKSEL_INTERNAL			= 0,
    MPU6500_CLKSEL_PLL				= 1
} PifMpu6500Clksel;

#define MPU6500_TEMP_DIS(N)			((N) << 3)	///< Puts N into the TEMP_DIS field
#define MPU6500_GYRO_STANDBY(N)		((N) << 4)	///< Puts N into the GYRO_STANDBY field
#define MPU6500_CYCLE(N)			((N) << 5)	///< Puts N into the CYCLE field
#define MPU6500_SLEEP(N)			((N) << 6)	///< Puts N into the SLEEP field
#define MPU6500_DEVICE_RESET(N)		((N) << 7)	///< Puts N into the DEVICE_RESET field

#define MPU6500_CLKSEL_MASK			0x07	///< Mask of the CLKSEL field
#define MPU6500_TEMP_DIS_MASK		0x08	///< Mask of the TEMP_DIS field
#define MPU6500_GYRO_STANDBY_MASK	0x10	///< Mask of the GYRO_STANDBY field
#define MPU6500_CYCLE_MASK			0x20	///< Mask of the CYCLE field
#define MPU6500_SLEEP_MASK			0x40	///< Mask of the SLEEP field
#define MPU6500_DEVICE_RESET_MASK	0x80	///< Mask of the DEVICE_RESET field
/** @} */


// Register : PWR_MGMT_2

/** @name MPU6500 PWR_MGMT_2 register
 *  @{
 */
#define MPU6500_DISABLE_ZG(N)		(N)	///< Puts N into the DISABLE_ZG field
#define MPU6500_DISABLE_YG(N)		((N) << 1)	///< Puts N into the DISABLE_YG field
#define MPU6500_DISABLE_XG(N)		((N) << 2)	///< Puts N into the DISABLE_XG field
#define MPU6500_DISABLE_ZA(N)		((N) << 3)	///< Puts N into the DISABLE_ZA field
#define MPU6500_DISABLE_YA(N)		((N) << 4)	///< Puts N into the DISABLE_YA field
#define MPU6500_DISABLE_XA(N)		((N) << 5)	///< Puts N into the DISABLE_XA field

// Bits 7:6 are reserved. The MPU-6050's LP_WAKE_CTRL became LP_ACCEL_ODR (see PifMpu6500LposcClksel).

#define MPU6500_DISABLE_ZG_MASK		0x01	///< Mask of the DISABLE_ZG field
#define MPU6500_DISABLE_YG_MASK		0x02	///< Mask of the DISABLE_YG field
#define MPU6500_DISABLE_XG_MASK		0x04	///< Mask of the DISABLE_XG field
#define MPU6500_DISABLE_ZA_MASK		0x08	///< Mask of the DISABLE_ZA field
#define MPU6500_DISABLE_YA_MASK		0x10	///< Mask of the DISABLE_YA field
#define MPU6500_DISABLE_XA_MASK		0x20	///< Mask of the DISABLE_XA field
/** @} */


/**
 * @class StPifMpu6500
 * @brief MPU-6500 gyroscope and accelerometer.
 */
typedef struct StPifMpu6500
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
} PifMpu6500;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads the ranges of the MPU-6500, wakes it up and registers it as the gyro and the accelerometer of an
 *        IMU sensor. Called by pifMpu6500I2c_Init() and pifMpu6500Spi_Init().
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_Config(PifMpu6500* p_owner, PifId id, PifImuSensor* p_imu_sensor);

/**
 * @brief Writes the GYRO_CONFIG register and updates the gyro gain of the IMU sensor to its range.
 * @param p_owner Pointer to the owner instance.
 * @param gyro_config Fields of the register, ORed together.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_SetGyroConfig(PifMpu6500* p_owner, uint8_t gyro_config);

/**
 * @brief Sets the gyro range and updates the gyro gain of the IMU sensor to it.
 * @param p_owner Pointer to the owner instance.
 * @param gyro_fs_sel Gyroscope full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_SetGyroFsSel(PifMpu6500* p_owner, PifMpu6500GyroFsSel gyro_fs_sel);

/**
 * @brief Writes the ACCEL_CONFIG register and updates the accelerometer gain of the IMU sensor to its range.
 * @param p_owner Pointer to the owner instance.
 * @param accel_config Fields of the register, ORed together.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_SetAccelConfig(PifMpu6500* p_owner, uint8_t accel_config);

/**
 * @brief Sets the accelerometer range and updates the accelerometer gain of the IMU sensor to it.
 * @param p_owner Pointer to the owner instance.
 * @param accel_fs_sel Accelerometer full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_SetAccelFsSel(PifMpu6500* p_owner, PifMpu6500AccelFsSel accel_fs_sel);

/**
 * @brief Reads the raw gyro values of the 3 axes, divided by gyro_scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_ReadGyro(PifMpu6500* p_owner, int16_t* p_gyro);

/**
 * @brief Reads the raw accelerometer values of the 3 axes, divided by accel_scale when it is not 0.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_ReadAccel(PifMpu6500* p_owner, int16_t* p_accel);

/**
 * @brief Reads the temperature in degrees C, multiplied by temp_scale.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Receives the temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_ReadTemperature(PifMpu6500* p_owner, int16_t* p_temperature);

/**
 * @brief Measures the gyro bias and noise at rest, taking a sample every 5 ms.
 *        Wraps pifImuSensor_CalibrateGyro().
 * @param p_owner Pointer to the owner instance.
 * @param samples Number of samples to average.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_CalibrationGyro(PifMpu6500* p_owner, uint8_t samples);

/**
 * @brief Sets the gyro deadband as a multiple of the measured noise, calibrating
 *        with 50 samples first if needed. Wraps pifImuSensor_SetGyroDeadband().
 * @param p_owner Pointer to the owner instance.
 * @param multiple Deadband width in noise standard deviations. 0 turns it off.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500_SetThreshold(PifMpu6500* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_H
