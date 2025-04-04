#ifndef PIF_MPU6500_H
#define PIF_MPU6500_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "sensor/pif_imu_sensor.h"


#define MPU6500_I2C_ADDR(N)			(0x68 + (N))

#define MPU6500_WHO_AM_I_CONST		0x70


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

typedef enum EnPifMpu6500ExtSyncSet
{
	MPU6500_EXT_SYNC_SET_DISABLE		= 0 << 3,
	MPU6500_EXT_SYNC_SET_TEMP_OUT_L		= 1 << 3,
	MPU6500_EXT_SYNC_SET_GYRO_XOUT_L	= 2 << 3,
	MPU6500_EXT_SYNC_SET_GYRO_YOUT_L	= 3 << 3,
	MPU6500_EXT_SYNC_SET_GYRO_ZOUT_L	= 4 << 3,
	MPU6500_EXT_SYNC_SET_ACCEL_XOUT_L	= 5 << 3,
    MPU6500_EXT_SYNC_SET_ACCEL_YOUT_L	= 6 << 3,
    MPU6500_EXT_SYNC_SET_ACCEL_ZOUT_	= 7 << 3L
} PifMpu6500ExtSyncSet;

#define MPU6500_FIFO_MODE(N)			((N) << 6)

#define MPU6500_DLPF_CFG_MASK			0x07
#define MPU6500_EXT_SYNC_SET_MASK		0x38
#define MPU6500_FIFO_MODE_MASK			0x40


// Register : GYRO_CONFIG

typedef enum EnPifMpu6500FchoiceB
{
    MPU6500_FCHOICE_B_DISABLE		= 0,
    MPU6500_FCHOICE_B_8800HZ		= 1,
    MPU6500_FCHOICE_B_3600HZ		= 2
} PifMpu6500FchoiceB;

typedef enum EnPifMpu6500GyroFsSel
{
    MPU6500_GYRO_FS_SEL_250DPS		= 0 << 3,
    MPU6500_GYRO_FS_SEL_500DPS		= 1 << 3,
    MPU6500_GYRO_FS_SEL_1000DPS		= 2 << 3,
    MPU6500_GYRO_FS_SEL_2000DPS		= 3 << 3
} PifMpu6500GyroFsSel;

#define MPU6500_ZG_ST(N)			((N) << 5)
#define MPU6500_YG_ST(N)			((N) << 6)
#define MPU6500_XG_ST(N)			((N) << 7)

#define MPU6500_FCHOICE_B_MASK		0x03
#define MPU6500_GYRO_FS_SEL_MASK	0x18		// Use pifMpu6500_SetGyroConfig or pifMpu6500_SetGyroFsSel to change this value.
#define MPU6500_ZG_ST_MASK			0x20
#define MPU6500_YG_ST_MASK			0x40
#define MPU6500_XG_ST_MASK			0x80


// Register : ACCEL_CONFIG

typedef enum EnPifMpu6500AccelFsSel
{
    MPU6500_ACCEL_FS_SEL_2G			= 0 << 3,
    MPU6500_ACCEL_FS_SEL_4G			= 1 << 3,
    MPU6500_ACCEL_FS_SEL_8G			= 2 << 3,
    MPU6500_ACCEL_FS_SEL_16G		= 3 << 3
} PifMpu6500AccelFsSel;

#define MPU6500_ZA_ST(N)			((N) << 5)
#define MPU6500_YA_ST(N)			((N) << 6)
#define MPU6500_XA_ST(N)			((N) << 7)

#define MPU6500_ACCEL_FS_SEL_MASK	0x18		// Use pifMpu6500_SetAccelConfig or pifMpu6500_SetAfsSel to change this value.
#define MPU6500_ZA_ST_MASK			0x20
#define MPU6500_YA_ST_MASK			0x40
#define MPU6500_XA_ST_MASK			0x80


// Register : ACCEL_CONFIG_2

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

#define MPU6500_ACCEL_FCHOICE_B(N)		((N) << 3)

#define MPU6500_A_DLPF_CFG_MASK			0x07
#define MPU6500_ACCEL_FCHOICE_B_MASK	0x08


// Register : LP_ACCEL_ODR

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

#define MPU6500_LPOSC_CLKSEL_MASK	0x0F


// Register : FIFO_EN

#define MPU6500_SLV_0(N)		(N)
#define MPU6500_SLV_1(N)		((N) << 1)
#define MPU6500_SLV_2(N)		((N) << 2)
#define MPU6500_ACCEL(N)		((N) << 3)
#define MPU6500_GYRO_ZOUT(N)	((N) << 4)
#define MPU6500_GYRO_YOUT(N)	((N) << 5)
#define MPU6500_GYRO_XOUT(N)	((N) << 6)
#define MPU6500_TEMP_OUT(N)		((N) << 7)

#define MPU6500_SLV_0_MASK		0x01
#define MPU6500_SLV_1_MASK		0x02
#define MPU6500_SLV_2_MASK		0x04
#define MPU6500_ACCEL_MASK		0x08
#define MPU6500_GYRO_ZOUT_MASK	0x10
#define MPU6500_GYRO_YOUT_MASK	0x20
#define MPU6500_GYRO_XOUT_MASK	0x40
#define MPU6500_TEMP_OUT_MASK	0x80


// Register : I2C_MST_CTRL

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

#define MPU6500_I2C_MST_P_NSR(N)	((N) << 4)
#define MPU6500_SLV_3_FIFO_EN(N)	((N) << 5)
#define MPU6500_WAIT_FOR_ES(N)		((N) << 6)
#define MPU6500_MULT_MST_EN(N)		((N) << 7)

#define MPU6500_I2C_MST_CLK_MASK	0x0F
#define MPU6500_I2C_MST_P_NSR_MASK	0x10
#define MPU6500_SLV_3_FIFO_EN_MASK	0x20
#define MPU6500_WAIT_FOR_ES_MASK	0x40
#define MPU6500_MULT_MST_EN_MASK	0x80


// Register : I2C_SLVx_ADDR		x = 0 ~ 4

#define MPU6500_I2C_ID_x(N)			(N)
#define MPU6500_I2C_SLVx_RNW(N)		((N) << 7)

#define MPU6500_I2C_ID_x_MASK		0x7F
#define MPU6500_I2C_SLVx_RNW_MASK	0x80


// Register : I2C_SLVx_CTRL		x = 0 ~ 3

#define MPU6500_I2C_SLVx_LENG(N)		(N)
#define MPU6500_I2C_SLVx_GRP(N)			((N) << 4)
#define MPU6500_I2C_SLVx_REG_DIS(N)		((N) << 5)
#define MPU6500_I2C_SLVx_BYTE_SW(N)		((N) << 6)
#define MPU6500_I2C_SLVx_EN(N)			((N) << 7)

#define MPU6500_I2C_SLVx_LENG_MASK		0x0F
#define MPU6500_I2C_SLVx_GRP_MASK		0x10
#define MPU6500_I2C_SLVx_REG_DIS_MASK	0x20
#define MPU6500_I2C_SLVx_BYTE_SW_MASK	0x40
#define MPU6500_I2C_SLVx_EN_MASK		0x80


// Register : I2C_SLV4_CTRL

#define MPU6500_I2C_MST_DLY(N)			(N)
#define MPU6500_I2C_SLV4_REG_DIS(N)		((N) << 5)
#define MPU6500_SLV4_DONE_INT_EN(N)		((N) << 6)
#define MPU6500_I2C_SLV4_EN(N)			((N) << 7)

#define MPU6500_I2C_MST_DLY_MASK		0x1F
#define MPU6500_I2C_SLV4_REG_DIS_MASK	0x20
#define MPU6500_SLV4_DONE_INT_EN_MASK	0x40
#define MPU6500_I2C_SLV4_EN_MASK		0x80


// Register : I2C_MST_STSTUS

#define MPU6500_I2C_SLV0_NACK(N)	(N)
#define MPU6500_I2C_SLV1_NACK(N)	((N) << 1)
#define MPU6500_I2C_SLV2_NACK(N)	((N) << 2)
#define MPU6500_I2C_SLV3_NACK(N)	((N) << 3)
#define MPU6500_I2C_SLV4_NACK(N)	((N) << 4)
#define MPU6500_I2C_LOST_ARB(N)		((N) << 5)
#define MPU6500_I2C_SLV4_DONE(N)	((N) << 6)
#define MPU6500_PASS_THROUGH(N)		((N) << 7)

#define MPU6500_I2C_SLV0_NACK_MASK	0x01
#define MPU6500_I2C_SLV1_NACK_MASK	0x02
#define MPU6500_I2C_SLV2_NACK_MASK	0x04
#define MPU6500_I2C_SLV3_NACK_MASK	0x08
#define MPU6500_I2C_SLV4_NACK_MASK	0x10
#define MPU6500_I2C_LOST_ARB_MASK	0x20
#define MPU6500_I2C_SLV4_DONE_MASK	0x40
#define MPU6500_PASS_THROUGH_MASK	0x80


// Register : INT_PIN_CFG

#define MPU6500_BYPASS_EN(N)			((N) << 1)
#define MPU6500_FSYNC_INT_MODE_EN(N)	((N) << 2)
#define MPU6500_ACTL_FSYNC(N)			((N) << 3)
#define MPU6500_INT_ANYRD_2CLEAR(N)		((N) << 4)
#define MPU6500_LATCH_INT_EN(N)			((N) << 5)
#define MPU6500_OPEN(N)					((N) << 6)
#define MPU6500_ACTL(N)					((N) << 7)

#define MPU6500_BYPASS_EN_MASK			0x02
#define MPU6500_FSYNC_INT_MODE_EN_MASK	0x04
#define MPU6500_ACTL_FSYNC_MASK			0x08
#define MPU6500_INT_ANYRD_2CLEAR_MASK	0x10
#define MPU6500_LATCH_INT_EN_MASK		0x20
#define MPU6500_OPEN_MASK				0x40
#define MPU6500_ACTL_MASK				0x80


// Register : INT_ENABLE

#define MPU6500_RAW_RDY_EN(N)			(N)
#define MPU6500_FSYNC_INT_EN(N)			((N) << 3)
#define MPU6500_FIFO_OVERFLOW_EN(N)		((N) << 4)
#define MPU6500_WOM_EN(N)				((N) << 6)

#define MPU6500_RAW_RDY_EN_MASK			0x01
#define MPU6500_FSYNC_INT_EN_MASK		0x08
#define MPU6500_FIFO_OVERFLOW_EN_MASK	0x10
#define MPU6500_WOM_EN_MASK				0x40


// Register : INT_STATUS

#define MPU6500_RAW_RDY_INT(N)			(N)
#define MPU6500_DMP_INT(N)				((N) << 1)
#define MPU6500_FSYNC_INT(N)			((N) << 3)
#define MPU6500_FIFO_OVERFLOW_INT(N)	((N) << 4)
#define MPU6500_WOM_INT(N)				((N) << 6)

#define MPU6500_RAW_RDY_INT_MASK		0x01
#define MPU6500_DMP_INT_MASK			0x02
#define MPU6500_FSYNC_INT_MASK			0x08
#define MPU6500_FIFO_OVERFLOW_INT_MASK	0x10
#define MPU6500_WOM_INT_MASK			0x40


// Register : I2C_MST_DELAY_CTRL

#define MPU6500_I2C_SLV0_DLY_EN(N)		(N)
#define MPU6500_I2C_SLV1_DLY_EN(N)		((N) << 1)
#define MPU6500_I2C_SLV2_DLY_EN(N)		((N) << 2)
#define MPU6500_I2C_SLV3_DLY_EN(N)		((N) << 3)
#define MPU6500_I2C_SLV4_DLY_EN(N)		((N) << 4)
#define MPU6500_DELAY_ES_SHADOW(N)		((N) << 7)

#define MPU6500_I2C_SLV0_DLY_EN_MASK	0x01
#define MPU6500_I2C_SLV1_DLY_EN_MASK	0x02
#define MPU6500_I2C_SLV2_DLY_EN_MASK	0x04
#define MPU6500_I2C_SLV3_DLY_EN_MASK	0x08
#define MPU6500_I2C_SLV4_DLY_EN_MASK	0x10
#define MPU6500_DELAY_ES_SHADOW_MASK	0x80


// Register : SIGNAL_PATH_RESET

#define MPU6500_TEMP_RESET(N)		(N)
#define MPU6500_ACCEL_RESET(N)		((N) << 1)
#define MPU6500_GYRO_RESET(N)		((N) << 2)

#define MPU6500_TEMP_RESET_MASK		0x01
#define MPU6500_ACCEL_RESET_MASK	0x02
#define MPU6500_GYRO_RESET_MASK		0x04


// Register : ACCEL_INTEL_CTRL

#define MPU6500_ACCEL_INTEL_MODE(N)		((N) << 6)
#define MPU6500_ACCEL_INTEL_EN(N)		((N) << 7)

#define MPU6500_ACCEL_INTEL_MODE_MASK	0x40
#define MPU6500_ACCEL_INTEL_EN_MASK		0x80


// Register : USER_CTRL

#define MPU6500_SIG_COND_RST(N)		(N)
#define MPU6500_I2C_MST_RST(N)		((N) << 1)
#define MPU6500_FIFO_RST(N)			((N) << 2)
#define MPU6500_DMP_RST(N)			((N) << 3)
#define MPU6500_I2C_IF_DIS(N)		((N) << 4)
#define MPU6500_I2C_MST_EN(N)		((N) << 5)
#define MPU6500_FIFO_EN(N)			((N) << 6)
#define MPU6500_DMP_EN(N)			((N) << 7)

#define MPU6500_SIG_COND_RST_MASK	0x01
#define MPU6500_I2C_MST_RST_MASK	0x02
#define MPU6500_FIFO_RST_MASK		0x04
#define MPU6500_DMP_RST_MASK		0x08
#define MPU6500_I2C_IF_DIS_MASK		0x10
#define MPU6500_I2C_MST_EN_MASK		0x20
#define MPU6500_FIFO_EN_MASK		0x40
#define MPU6500_DMP_EN_MASK			0x80


// Register : PWR_MGMT_1

typedef enum EnPifMpu6500Clksel
{
    MPU6500_CLKSEL_INTERNAL			= 0,
    MPU6500_CLKSEL_PLL				= 1
} PifMpu6500Clksel;

#define MPU6500_TEMP_DIS(N)			((N) << 3)
#define MPU6500_GYRO_STANDBY(N)		((N) << 4)
#define MPU6500_CYCLE(N)			((N) << 5)
#define MPU6500_SLEEP(N)			((N) << 6)
#define MPU6500_DEVICE_RESET(N)		((N) << 7)

#define MPU6500_CLKSEL_MASK			0x07
#define MPU6500_TEMP_DIS_MASK		0x08
#define MPU6500_GYRO_STANDBY_MASK	0x10
#define MPU6500_CYCLE_MASK			0x20
#define MPU6500_SLEEP_MASK			0x40
#define MPU6500_DEVICE_RESET_MASK	0x80


// Register : PWR_MGMT_2

#define MPU6500_DISABLE_ZG(N)		(N)
#define MPU6500_DISABLE_YG(N)		((N) << 1)
#define MPU6500_DISABLE_XG(N)		((N) << 2)
#define MPU6500_DISABLE_ZA(N)		((N) << 3)
#define MPU6500_DISABLE_YA(N)		((N) << 4)
#define MPU6500_DISABLE_XA(N)		((N) << 5)

typedef enum EnPifMpu6500LpWakeCtrl
{
    MPU6500_LP_WAKE_CTRL_1_25HZ		= 0 << 6,
    MPU6500_LP_WAKE_CTRL_5HZ		= 1 << 6,
    MPU6500_LP_WAKE_CTRL_20HZ		= 2 << 6,
    MPU6500_LP_WAKE_CTRL_40HZ		= 3 << 6
} PifMpu6500LpWakeCtrl;

#define MPU6500_DISABLE_ZG_MASK		0x01
#define MPU6500_DISABLE_YG_MASK		0x02
#define MPU6500_DISABLE_XG_MASK		0x04
#define MPU6500_DISABLE_ZA_MASK		0x08
#define MPU6500_DISABLE_YA_MASK		0x10
#define MPU6500_DISABLE_XA_MASK		0x20
#define MPU6500_LP_WAKE_CTRL_MASK	0xC0


/**
 * @class StPifMpu6500
 * @brief
 */
typedef struct StPifMpu6500
{
	// Public Member Variable
	uint8_t gyro_scale;
	uint8_t accel_scale;
	uint8_t temp_scale;

	// Read-only Member Variable
	PifId _id;
	union {
		PifI2cDevice* _p_i2c;
		PifSpiDevice* _p_spi;
	};

	// Read-only Function
	PifDeviceReg8Func _fn;

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifMpu6500;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu6500_Config
 * @brief
 * @param p_owner
 * @param id
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu6500_Config(PifMpu6500* p_owner, PifId id, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu6500_SetGyroConfig
 * @brief
 * @param p_owner
 * @param gyro_config
 * @return
 */
BOOL pifMpu6500_SetGyroConfig(PifMpu6500* p_owner, uint8_t gyro_config);

/**
 * @fn pifMpu6500_SetGyroFsSel
 * @brief
 * @param p_owner
 * @param gyro_fs_sel
 * @return
 */
BOOL pifMpu6500_SetGyroFsSel(PifMpu6500* p_owner, PifMpu6500GyroFsSel gyro_fs_sel);

/**
 * @fn pifMpu6500_SetAccelConfig
 * @brief
 * @param p_owner
 * @param accel_config
 * @return
 */
BOOL pifMpu6500_SetAccelConfig(PifMpu6500* p_owner, uint8_t accel_config);

/**
 * @fn pifMpu6500_SetaAccelFsSel
 * @brief
 * @param p_owner
 * @param accel_fs_sel
 * @return
 */
BOOL pifMpu6500_SetaAccelFsSel(PifMpu6500* p_owner, PifMpu6500AccelFsSel accel_fs_sel);

/**
 * @fn pifMpu6500_ReadGyro
 * @brief
 * @param p_owner
 * @param p_gyro
 * @return
 */
BOOL pifMpu6500_ReadGyro(PifMpu6500* p_owner, int16_t* p_gyro);

/**
 * @fn pifMpu6500_ReadAccel
 * @brief
 * @param p_owner
 * @param p_accel
 * @return
 */
BOOL pifMpu6500_ReadAccel(PifMpu6500* p_owner, int16_t* p_accel);

/**
 * @fn pifMpu6500_ReadTemperature
 * @brief
 * @param p_owner
 * @param p_temperature
 * @return
 */
BOOL pifMpu6500_ReadTemperature(PifMpu6500* p_owner, int16_t* p_temperature);

/**
 * @fn pifMpu6500_CalibrationGyro
 * @brief
 * @param p_owner
 * @param samples
 * @return
 */
BOOL pifMpu6500_CalibrationGyro(PifMpu6500* p_owner, uint8_t samples);

/**
 * @fn pifMpu6500_SetThreshold
 * @brief
 * @param p_owner
 * @param multiple
 * @return
 */
BOOL pifMpu6500_SetThreshold(PifMpu6500* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_H
