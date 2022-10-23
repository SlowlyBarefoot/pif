#ifndef PIF_MPU6500_H
#define PIF_MPU6500_H


#include "core/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define MPU6500_I2C_ADDR(N)			(0x68 + (N))


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
	MPU6500_DLPF_CFG_250HZ,
	MPU6500_DLPF_CFG_184HZ,
	MPU6500_DLPF_CFG_92HZ,
	MPU6500_DLPF_CFG_41HZ,
	MPU6500_DLPF_CFG_20HZ,
	MPU6500_DLPF_CFG_10HZ,
    MPU6500_DLPF_CFG_5HZ,
	MPU6500_DLPF_CFG_3600HZ
} PifMpu6500DlpfCfg;

typedef enum EnPifMpu6500ExtSyncSet
{
	MPU6500_EXT_SYNC_SET_DISABLE,
	MPU6500_EXT_SYNC_SET_TEMP_OUT_L,
	MPU6500_EXT_SYNC_SET_GYRO_XOUT_L,
	MPU6500_EXT_SYNC_SET_GYRO_YOUT_L,
	MPU6500_EXT_SYNC_SET_GYRO_ZOUT_L,
	MPU6500_EXT_SYNC_SET_ACCEL_XOUT_L,
    MPU6500_EXT_SYNC_SET_ACCEL_YOUT_L,
    MPU6500_EXT_SYNC_SET_ACCEL_ZOUT_L
} PifMpu6500ExtSyncSet;

#define MPU6500_CONFIG_DLPF_CFG			0x0003
#define MPU6500_CONFIG_EXT_SYNC_SET		0x0303
#define MPU6500_CONFIG_FIFO_MODE		0x0601

typedef union StPifMpu6500Config
{
	uint8_t byte;
	struct {
		PifMpu6500DlpfCfg dlpf_cfg			: 3;	// LSB
		PifMpu6500ExtSyncSet ext_sync_set	: 3;
		uint8_t fifo_mode					: 1;
		uint8_t reserved					: 1;	// MSB
	} bit;
} PifMpu6500Config;


// Register : GYRO_CONFIG

typedef enum EnPifMpu6500GyroFsSel
{
    MPU6500_GYRO_FS_SEL_250DPS,
    MPU6500_GYRO_FS_SEL_500DPS,
    MPU6500_GYRO_FS_SEL_1000DPS,
    MPU6500_GYRO_FS_SEL_2000DPS
} PifMpu6500GyroFsSel;

#define MPU6500_GYRO_CONFIG_FCHOICE_B		0x0002
#define MPU6500_GYRO_CONFIG_GYRO_FS_SEL		0x0302		// Use pifMpu6500_SetGyroConfig or pifMpu6500_SetGyroFsSel to change this value.
#define MPU6500_GYRO_CONFIG_ZG_ST			0x0501
#define MPU6500_GYRO_CONFIG_YG_ST			0x0601
#define MPU6500_GYRO_CONFIG_XG_ST			0x0701

typedef union StPifMpu6500GyroConfig
{
	uint8_t byte;
	struct {
		uint8_t fchoice_b				: 2;	// LSB
		uint8_t reserved				: 1;
		PifMpu6500GyroFsSel gyro_fs_sel	: 2;	// Use pifMpu6500_SetGyroConfig or pifMpu6500_SetGyroFsSel to change this value.
		uint8_t zg_st					: 1;
		uint8_t yg_st					: 1;
		uint8_t xg_st					: 1;	// MSB
	} bit;
} PifMpu6500GyroConfig;


// Register : ACCEL_CONFIG

typedef enum EnPifMpu6500AccelFsSel
{
    MPU6500_ACCEL_FS_SEL_2G,
    MPU6500_ACCEL_FS_SEL_4G,
    MPU6500_ACCEL_FS_SEL_8G,
    MPU6500_ACCEL_FS_SEL_16G
} PifMpu6500AccelFsSel;

#define MPU6500_ACCEL_CONFIG_ACCEL_FS_SEL	0x0302		// Use pifMpu6500_SetAccelConfig or pifMpu6500_SetAfsSel to change this value.
#define MPU6500_ACCEL_CONFIG_ZA_ST			0x0501
#define MPU6500_ACCEL_CONFIG_YA_ST			0x0601
#define MPU6500_ACCEL_CONFIG_XA_ST			0x0701

typedef union StPifMpu6500AccelConfig
{
	uint8_t byte;
	struct {
		uint8_t reserved					: 3;	// LSB
		PifMpu6500AccelFsSel accel_fs_sel	: 2;	// Use pifMpu6500_SetAccelConfig or pifMpu6500_SetAfsSel to change this value.
		uint8_t za_st						: 1;
		uint8_t ya_st						: 1;
		uint8_t xa_st						: 1;	// MSB
	} bit;
} PifMpu6500AccelConfig;


// Register : ACCEL_CONFIG_2

typedef enum EnPifMpu6500ADlpfCfg
{
    MPU6500_A_DLPF_CFG_460HZ,
    MPU6500_A_DLPF_CFG_184HZ,
    MPU6500_A_DLPF_CFG_92HZ,
    MPU6500_A_DLPF_CFG_41HZ,
    MPU6500_A_DLPF_CFG_20HZ,
    MPU6500_A_DLPF_CFG_10HZ,
    MPU6500_A_DLPF_CFG_5HZ
} PifMpu6500ADlpfCfg;

#define MPU6500_ACCEL_CONFIG_2_A_DLPF_CFG		0x0003
#define MPU6500_ACCEL_CONFIG_2_ACCEL_FCHOICE_B	0x0301

typedef union StPifMpu6500AccelConfig2
{
	uint8_t byte;
	struct {
		PifMpu6500ADlpfCfg a_dlpf_cfg	: 3;	// LSB
		uint8_t accel_fchoice_b			: 1;
		uint8_t reserved				: 4;	// MSB
	} bit;
} PifMpu6500AccelConfig2;


// Register : LP_ACCEL_ODR

typedef enum EnPifMpu6500LposcClksel
{
    MPU6500_LPOSC_CLKSEL_0_24,
    MPU6500_LPOSC_CLKSEL_0_49,
    MPU6500_LPOSC_CLKSEL_0_98,
    MPU6500_LPOSC_CLKSEL_1_95,
    MPU6500_LPOSC_CLKSEL_3_91,
    MPU6500_LPOSC_CLKSEL_7_81,
    MPU6500_LPOSC_CLKSEL_15_63,
    MPU6500_LPOSC_CLKSEL_31_25,
    MPU6500_LPOSC_CLKSEL_62_50,
    MPU6500_LPOSC_CLKSEL_125,
    MPU6500_LPOSC_CLKSEL_250,
    MPU6500_LPOSC_CLKSEL_500
} PifMpu6500LposcClksel;

#define MPU6500_LP_ACCEL_ODR_LPOSC_CLKSEL	0x0004

typedef union StPifMpu6500LpAccelOdr
{
	uint8_t byte;
	struct {
		PifMpu6500ADlpfCfg lposc_clksel	: 4;	// LSB
		uint8_t reserved				: 4;	// MSB
	} bit;
} PifMpu6500LpAccelOdr;


// Register : FIFO_EN

#define MPU6500_FIFO_EN_SLV_0				0x0001
#define MPU6500_FIFO_EN_SLV_1				0x0101
#define MPU6500_FIFO_EN_SLV_2				0x0201
#define MPU6500_FIFO_EN_ACCEL				0x0301
#define MPU6500_FIFO_EN_GYRO_ZOUT			0x0401
#define MPU6500_FIFO_EN_GYRO_YOUT			0x0501
#define MPU6500_FIFO_EN_GYRO_XOUT			0x0601
#define MPU6500_FIFO_EN_TEMP_OUT			0x0701

typedef union StPifMpu6500FifoEn
{
	uint8_t byte;
	struct {
		uint8_t slv_0			: 1;	// LSB
		uint8_t slv_1			: 1;
		uint8_t slv_2			: 1;
		uint8_t accel			: 1;
		uint8_t gyro_zout		: 1;
		uint8_t gyro_yout		: 1;
		uint8_t gyro_xout		: 1;
		uint8_t temp_out		: 1;	// MSB
	} bit;
} PifMpu6500FifoEn;


// Register : I2C_MST_CTRL

typedef enum EnPifMpu6500I2cMstClk
{
    MPU6500_I2C_MST_CLK_348KHZ,
    MPU6500_I2C_MST_CLK_333KHZ,
    MPU6500_I2C_MST_CLK_320KHZ,
    MPU6500_I2C_MST_CLK_308KHZ,
    MPU6500_I2C_MST_CLK_296KHZ,
    MPU6500_I2C_MST_CLK_286KHZ,
    MPU6500_I2C_MST_CLK_276KHZ,
    MPU6500_I2C_MST_CLK_267KHZ,
    MPU6500_I2C_MST_CLK_258KHZ,
    MPU6500_I2C_MST_CLK_500KHZ,
    MPU6500_I2C_MST_CLK_471KHZ,
    MPU6500_I2C_MST_CLK_444KHZ,
    MPU6500_I2C_MST_CLK_421KHZ,
    MPU6500_I2C_MST_CLK_400KHZ,
    MPU6500_I2C_MST_CLK_381KHZ,
    MPU6500_I2C_MST_CLK_364KHZ
} PifMpu6500I2cMstClk;

#define MPU6500_I2C_MST_CTRL_I2C_MST_CLK	0x0004
#define MPU6500_I2C_MST_CTRL_I2C_MST_P_NSR	0x0401
#define MPU6500_I2C_MST_CTRL_SLV_3_FIFO_EN	0x0501
#define MPU6500_I2C_MST_CTRL_WAIT_FOR_ES	0x0601
#define MPU6500_I2C_MST_CTRL_MULT_MST_EN	0x0701

typedef union StPifMpu6500I2cMstCtrl
{
	uint8_t byte;
	struct {
		PifMpu6500I2cMstClk i2c_mst_clk		: 4;	// LSB
		uint8_t i2c_mst_p_nsr				: 1;
		uint8_t slv_3_fifo_en				: 1;
		uint8_t wait_for_es					: 1;
		uint8_t mult_mst_en					: 1;	// MSB
	} bit;
} PifMpu6500I2cMstCtrl;


// Register : I2C_SLVx_ADDR		x = 0 ~ 4

#define MPU6500_I2C_SLV_ADDR_I2C_ID_x			0x0007
#define MPU6500_I2C_SLV_ADDR_I2C_SLVx_RNW		0x0701

typedef union StPifMpu6500I2cSlvxAddr
{
	uint8_t byte;
	struct {
		uint8_t i2c_id_x		: 7;	// LSB
		uint8_t i2c_slvx_rnw	: 1;	// MSB
	} bit;
} PifMpu6500I2cSlvxAddr;


// Register : I2C_SLVx_CTRL		x = 0 ~ 3

#define MPU6500_I2C_SLV_CTRL_I2C_SLVx_LENG		0x0004
#define MPU6500_I2C_SLV_CTRL_I2C_SLVx_GRP		0x0401
#define MPU6500_I2C_SLV_CTRL_I2C_SLVx_REG_DIS	0x0501
#define MPU6500_I2C_SLV_CTRL_I2C_SLVx_BYTE_SW	0x0601
#define MPU6500_I2C_SLV_CTRL_I2C_SLVx_EN		0x0701

typedef union StPifMpu6500I2cSlvxCtrl
{
	uint8_t byte;
	struct {
		uint8_t i2c_slvx_leng		: 4;	// LSB
		uint8_t i2c_slvx_grp		: 1;
		uint8_t i2c_slvx_reg_dis	: 1;
		uint8_t i2c_slvx_byte_sw	: 1;
		uint8_t i2c_slvx_en			: 1;	// MSB
	} bit;
} PifMpu6500I2cSlvxCtrl;


// Register : I2C_SLV4_CTRL

#define MPU6500_I2C_SLV_CTRL_I2C_MST_DLY		0x0005
#define MPU6500_I2C_SLV_CTRL_I2C_SLV4_REG_DIS	0x0501
#define MPU6500_I2C_SLV_CTRL_SLV4_DONE_INT_EN	0x0601
#define MPU6500_I2C_SLV_CTRL_I2C_SLV4_EN		0x0701

typedef union StPifMpu6500I2cSlv4Ctrl
{
	uint8_t byte;
	struct {
		uint8_t i2c_mst_dly			: 5;	// LSB
		uint8_t i2c_slv4_reg_dis	: 1;
		uint8_t slv4_done_int_en	: 1;
		uint8_t i2c_slv4_en			: 1;	// MSB
	} bit;
} PifMpu6500I2cSlv4Ctrl;


// Register : I2C_MST_STSTUS

#define MPU6500_I2C_MST_STATUS_I2C_SLV0_NACK	0x0001
#define MPU6500_I2C_MST_STATUS_I2C_SLV1_NACK	0x0101
#define MPU6500_I2C_MST_STATUS_I2C_SLV2_NACK	0x0201
#define MPU6500_I2C_MST_STATUS_I2C_SLV3_NACK	0x0301
#define MPU6500_I2C_MST_STATUS_I2C_SLV4_NACK	0x0401
#define MPU6500_I2C_MST_STATUS_I2C_LOST_ARB		0x0501
#define MPU6500_I2C_MST_STATUS_I2C_SLV4_DONE	0x0601
#define MPU6500_I2C_MST_STATUS_PASS_THROUGH		0x0701

typedef union StPifMpu6500I2cMstStatus
{
	uint8_t byte;
	struct {
		uint8_t i2c_slv0_nack	: 1;	// LSB
		uint8_t i2c_slv1_nack	: 1;
		uint8_t i2c_slv2_nack	: 1;
		uint8_t i2c_slv3_nack	: 1;
		uint8_t i2c_slv4_nack	: 1;
		uint8_t i2c_lost_arb	: 1;
		uint8_t i2c_slv4_done	: 1;
		uint8_t pass_through	: 1;	// MSB
	} bit;
} PifMpu6500I2cMstStatus;


// Register : INT_PIN_CFG

#define MPU6500_INT_PIN_CFG_BYPASS_EN			0x0101
#define MPU6500_INT_PIN_CFG_FSYNC_INT_MODE_EN	0x0201
#define MPU6500_INT_PIN_CFG_ACTL_FSYNC			0x0301
#define MPU6500_INT_PIN_CFG_INT_ANYRD_2CLEAR	0x0401
#define MPU6500_INT_PIN_CFG_LATCH_INT_EN		0x0501
#define MPU6500_INT_PIN_CFG_OPEN				0x0601
#define MPU6500_INT_PIN_CFG_ACTL				0x0701

typedef union StPifMpu6500IntPinCfg
{
	uint8_t byte;
	struct {
		uint8_t reserved			: 1;	// LSB
		uint8_t bypass_en			: 1;
		uint8_t fsync_int_mode_en	: 1;
		uint8_t actl_fsync			: 1;
		uint8_t int_adyrd_2clear	: 1;
		uint8_t latch_int_en		: 1;
		uint8_t open				: 1;
		uint8_t actl				: 1;	// MSB
	} bit;
} PifMpu6500IntPinCfg;


// Register : INT_ENABLE

#define MPU6500_INT_ENABLE_RAW_RDY_EN			0x0001
#define MPU6500_INT_ENABLE_FSYNC_INT_EN			0x0301
#define MPU6500_INT_ENABLE_FIFO_OVERFLOW_EN		0x0401
#define MPU6500_INT_ENABLE_WOM_EN				0x0401

typedef union StPifMpu6500IntEnable
{
	uint8_t byte;
	struct {
		uint8_t raw_rdy_en			: 1;	// LSB
		uint8_t reserved1			: 2;
		uint8_t fsync_int_en		: 1;
		uint8_t fifo_overflow_en	: 1;
		uint8_t reserved2			: 1;
		uint8_t wom_en				: 1;
		uint8_t reserved3			: 1;	// MSB
	} bit;
} PifMpu6500IntEnable;


// Register : INT_STATUS

#define MPU6500_INT_STATUS_RAW_RDY_INT			0x0001
#define MPU6500_INT_STATUS_DMP_INT				0x0101
#define MPU6500_INT_STATUS_FSYNC_INT			0x0301
#define MPU6500_INT_STATUS_FIFO_OVERFLOW_INT	0x0401
#define MPU6500_INT_STATUS_WOM_INT				0x0601

typedef union StPifMpu6500IntStatus
{
	uint8_t byte;
	struct {
		uint8_t raw_rdy_int			: 1;	// LSB
		uint8_t dmp_int				: 1;
		uint8_t reserved1			: 1;
		uint8_t fsync_int			: 1;
		uint8_t fifo_overflow_int	: 1;
		uint8_t reserved2			: 1;
		uint8_t wom_int				: 1;
		uint8_t reserved3			: 1;	// MSB
	} bit;
} PifMpu6500IntStatus;


// Register : I2C_MST_DELAY_CTRL

#define MPU6500_I2C_MST_DELAY_CTRL_I2C_SLV0_DLY_EN	0x0001
#define MPU6500_I2C_MST_DELAY_CTRL_I2C_SLV1_DLY_EN	0x0101
#define MPU6500_I2C_MST_DELAY_CTRL_I2C_SLV2_DLY_EN	0x0201
#define MPU6500_I2C_MST_DELAY_CTRL_I2C_SLV3_DLY_EN	0x0301
#define MPU6500_I2C_MST_DELAY_CTRL_I2C_SLV4_DLY_EN	0x0401
#define MPU6500_I2C_MST_DELAY_CTRL_DELAY_ES_SHADOW	0x0701

typedef union StPifMpu6500I2cMstDelayCtrl
{
	uint8_t byte;
	struct {
		uint8_t i2c_slv0_dly_en		: 1;	// LSB
		uint8_t i2c_slv1_dly_en		: 1;
		uint8_t i2c_slv2_dly_en		: 1;
		uint8_t i2c_slv3_dly_en		: 1;
		uint8_t i2c_slv4_dly_en		: 1;
		uint8_t reserved			: 2;
		uint8_t delay_es_shadow		: 1;	// MSB
	} bit;
} PifMpu6500I2cMstDelayCtrl;


// Register : SIGNAL_PATH_RESET

#define MPU6500_SIGNAL_PATH_RESET_TEMP_RESET		0x0001
#define MPU6500_SIGNAL_PATH_RESET_ACCEL_RESET		0x0101
#define MPU6500_SIGNAL_PATH_RESET_GYRO_RESET		0x0201

typedef union StPifMpu6500SignalPathReset
{
	uint8_t byte;
	struct {
		uint8_t temp_reset		: 1;	// LSB
		uint8_t accel_reset		: 1;
		uint8_t gyro_reset		: 1;
		uint8_t reserved		: 5;	// MSB
	} bit;
} PifMpu6500SignalPathReset;


// Register : ACCEL_INTEL_CTRL

#define MPU6500_ACCEL_INTEL_CTRL_ACCEL_INTEL_MODE	0x0601
#define MPU6500_ACCEL_INTEL_CTRL_ACCEL_INTEL_EN		0x0701

typedef union StPifMpu6500AccelIntelCtrl
{
	uint8_t byte;
	struct {
		uint8_t reserved			: 6;	// LSB
		uint8_t accel_intel_mode	: 1;
		uint8_t accel_intel_en		: 1;	// MSB
	} bit;
} PifMpu6500AccelIntelCtrl;


// Register : USER_CTRL

#define MPU6500_USER_CTRL_SIG_COND_RST		0x0001
#define MPU6500_USER_CTRL_I2C_MST_RST		0x0101
#define MPU6500_USER_CTRL_FIFO_RST			0x0201
#define MPU6500_USER_CTRL_DMP_RST			0x0301
#define MPU6500_USER_CTRL_I2C_IF_DIS		0x0401
#define MPU6500_USER_CTRL_I2C_MST_EN		0x0501
#define MPU6500_USER_CTRL_FIFO_EN			0x0601
#define MPU6500_USER_CTRL_DMP_EN			0x0701

typedef union StPifMpu6500UserCtrl
{
	uint8_t byte;
	struct {
		uint8_t sig_cond_rst		: 1;	// LSB
		uint8_t i2c_mst_rst			: 1;
		uint8_t fifo_rst			: 1;
		uint8_t dmp_rst				: 1;
		uint8_t i2c_if_dis			: 1;
		uint8_t i2c_mst_en			: 1;
		uint8_t fifo_en				: 1;
		uint8_t dmp_en				: 1;	// MSB
	} bit;
} PifMpu6500UserCtrl;


// Register : PWR_MGMT_1

#define MPU6500_PWR_MGMT_1_CLKSEL			0x0003
#define MPU6500_PWR_MGMT_1_TEMP_DIS			0x0301
#define MPU6500_PWR_MGMT_1_GYRO_STANDBY		0x0401
#define MPU6500_PWR_MGMT_1_CYCLE			0x0501
#define MPU6500_PWR_MGMT_1_SLEEP			0x0601
#define MPU6500_PWR_MGMT_1_DEVICE_RESET		0x0701

typedef union StPifMpu6500PwrMgmt1
{
	uint8_t byte;
	struct {
		uint8_t clksel				: 3;	// LSB
		uint8_t temp_dis			: 1;
		uint8_t gyro_standby		: 1;
		uint8_t cycle				: 1;
		uint8_t sleep				: 1;
		uint8_t device_reset		: 1;	// MSB
	} bit;
} PifMpu6500PwrMgmt1;


// Register : PWR_MGMT_2

typedef enum EnPifMpu6500LpWakeCtrl
{
    MPU6500_LP_WAKE_CTRL_1_25HZ,
    MPU6500_LP_WAKE_CTRL_5HZ,
    MPU6500_LP_WAKE_CTRL_20HZ,
    MPU6500_LP_WAKE_CTRL_40HZ
} PifMpu6500LpWakeCtrl;

#define MPU6500_PWR_MGMT_2_DISABLE_ZG		0x0001
#define MPU6500_PWR_MGMT_2_DISABLE_YG		0x0101
#define MPU6500_PWR_MGMT_2_DISABLE_XG		0x0201
#define MPU6500_PWR_MGMT_2_DISABLE_ZA		0x0301
#define MPU6500_PWR_MGMT_2_DISABLE_YA		0x0401
#define MPU6500_PWR_MGMT_2_DISABLE_XA		0x0501
#define MPU6500_PWR_MGMT_2_LP_WAKE_CTRL		0x0602

typedef union StPifMpu6500PwrMgmt2
{
	uint8_t byte;
	struct {
		uint8_t disable_zg					: 1;	// LSB
		uint8_t disable_yg					: 1;
		uint8_t disable_xg					: 1;
		uint8_t disable_za					: 1;
		uint8_t disable_ya					: 1;
		uint8_t disable_xa					: 1;
		PifMpu6500LpWakeCtrl lp_wake_ctrl	: 2;	// MSB
	} bit;
} PifMpu6500PwrMgmt2;


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
	PifI2cDevice* _p_i2c;

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifMpu6500;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu6500_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu6500_Init(PifMpu6500* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu6500_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifMpu6500_Clear(PifMpu6500* p_owner);

/**
 * @fn pifMpu6500_SetGyroConfig
 * @brief
 * @param p_owner
 * @param gyro_config
 * @return
 */
BOOL pifMpu6500_SetGyroConfig(PifMpu6500* p_owner, PifMpu6500GyroConfig gyro_config);

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
BOOL pifMpu6500_SetAccelConfig(PifMpu6500* p_owner, PifMpu6500AccelConfig accel_config);

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
