#ifndef PIF_MPU60X0_H
#define PIF_MPU60X0_H


#include "core/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define MPU60X0_I2C_ADDR(N)			(0x68 + (N))


typedef enum EnPifMpu60x0Reg
{
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

#define MPU60X0_SELF_TEST_G_TEST		0x0005
#define MPU60X0_SELF_TEST_A_TEST		0x0503

typedef union StPifMpu60x0SelfTest
{
	uint8_t byte;
	struct {
		uint8_t g_test		: 5;	// LSB
		uint8_t a_test		: 3;	// MSB
	} bit;
} PifMpu60x0SelfTest;


// Register : SELF_TEST_A

#define MPU60X0_SELF_TEST_A_ZA_TEST		0x0002
#define MPU60X0_SELF_TEST_A_YA_TEST		0x0202
#define MPU60X0_SELF_TEST_A_XA_TEST		0x0402

typedef union StPifMpu60x0SelfTestA
{
	uint8_t byte;
	struct {
		uint8_t za_test		: 2;	// LSB
		uint8_t ya_test		: 2;
		uint8_t xa_test		: 2;
		uint8_t reserved	: 2;	// MSB
	} bit;
} PifMpu60x0SelfTestA;


// Register : CONFIG

typedef enum EnPifMpu60x0DlpfCfg
{
	MPU60X0_DLPF_CFG_A260HZ_G256HZ,		// This is the default setting, no need to uncomment, just for reference
	MPU60X0_DLPF_CFG_A184HZ_G188HZ,
	MPU60X0_DLPF_CFG_A94HZ_G98HZ,
	MPU60X0_DLPF_CFG_A44HZ_G42HZ,
	MPU60X0_DLPF_CFG_A21HZ_G20HZ,
	MPU60X0_DLPF_CFG_A10HZ_G10HZ,
    MPU60X0_DLPF_CFG_A5HZ_G5HZ      	// Use this only in extreme cases, rather change motors and/or props
} PifMpu60x0DlpfCfg;

typedef enum EnPifMpu60x0ExtSyncSet
{
	MPU60X0_EXT_SYNC_SET_INPUT_DISABLE,
	MPU60X0_EXT_SYNC_SET_TEMP_OUT,
	MPU60X0_EXT_SYNC_SET_GYRO_XOUT,
	MPU60X0_EXT_SYNC_SET_GYRO_YOUT,
	MPU60X0_EXT_SYNC_SET_GYRO_ZOUT,
	MPU60X0_EXT_SYNC_SET_ACCEL_XOUT,
    MPU60X0_EXT_SYNC_SET_ACCEL_YOUT,
    MPU60X0_EXT_SYNC_SET_ACCEL_ZOUT
} PifMpu60x0ExtSyncSet;

#define MPU60X0_CONFIG_DLPF_CFG			0x0003
#define MPU60X0_CONFIG_EXT_SYNC_SET		0x0303

typedef union StPifMpu60x0Config
{
	uint8_t byte;
	struct {
		PifMpu60x0DlpfCfg dlpf_cfg			: 3;	// LSB
		PifMpu60x0ExtSyncSet ext_sync_set	: 3;
		uint8_t reserved					: 2;	// MSB
	} bit;
} PifMpu60x0Config;


// Register : GYRO_CONFIG

typedef enum EnPifMpu60x0FsSel
{
    MPU60X0_FS_SEL_250DPS,
    MPU60X0_FS_SEL_500DPS,
    MPU60X0_FS_SEL_1000DPS,
    MPU60X0_FS_SEL_2000DPS
} PifMpu60x0FsSel;

#define MPU60X0_GYRO_CONFIG_FS_SEL		0x0302		// Use pifMpu60x0_SetGyroConfig or pifMpu60x0_SetFsSel to change this value.
#define MPU60X0_GYRO_CONFIG_ZG_ST		0x0501
#define MPU60X0_GYRO_CONFIG_YG_ST		0x0601
#define MPU60X0_GYRO_CONFIG_XG_ST		0x0701

typedef union StPifMpu60x0GyroConfig
{
	uint8_t byte;
	struct {
		uint8_t reserved		: 3;	// LSB
		PifMpu60x0FsSel fs_sel	: 2;	// Use pifMpu60x0_SetGyroConfig or pifMpu60x0_SetFsSel to change this value.
		uint8_t zg_st			: 1;
		uint8_t yg_st			: 1;
		uint8_t xg_st			: 1;	// MSB
	} bit;
} PifMpu60x0GyroConfig;


// Register : ACCEL_CONFIG

typedef enum EnPifMpu60x0AfsSel
{
    MPU60X0_AFS_SEL_2G,
    MPU60X0_AFS_SEL_4G,
    MPU60X0_AFS_SEL_8G,
    MPU60X0_AFS_SEL_16G
} PifMpu60x0AfsSel;

#define MPU60X0_ACCEL_CONFIG_AFS_SEL	0x0302		// Use pifMpu60x0_SetAccelConfig or pifMpu60x0_SetAfsSel to change this value.
#define MPU60X0_ACCEL_CONFIG_ZA_ST		0x0501
#define MPU60X0_ACCEL_CONFIG_YA_ST		0x0601
#define MPU60X0_ACCEL_CONFIG_XA_ST		0x0701

typedef union StPifMpu60x0AccelConfig
{
	uint8_t byte;
	struct {
		uint8_t reserved			: 3;	// LSB
		PifMpu60x0AfsSel afs_sel	: 2;	// Use pifMpu60x0_SetAccelConfig or pifMpu60x0_SetAfsSel to change this value.
		uint8_t zg_st				: 1;
		uint8_t yg_st				: 1;
		uint8_t xg_st				: 1;	// MSB
	} bit;
} PifMpu60x0AccelConfig;


// Register : FIFO_EN

#define MPU60X0_FIFO_EN_SLV0_FIFO_EN		0x0001
#define MPU60X0_FIFO_EN_SLV1_FIFO_EN		0x0101
#define MPU60X0_FIFO_EN_SLV2_FIFO_EN		0x0201
#define MPU60X0_FIFO_EN_ACCEL_FIFO_EN		0x0301
#define MPU60X0_FIFO_EN_ZG_FIFO_EN			0x0401
#define MPU60X0_FIFO_EN_YG_FIFO_EN			0x0501
#define MPU60X0_FIFO_EN_XG_FIFO_EN			0x0601
#define MPU60X0_FIFO_EN_TEMP_FIFO_EN		0x0701

typedef union StPifMpu60x0FifoEn
{
	uint8_t byte;
	struct {
		uint8_t slv0_fifo_en	: 1;	// LSB
		uint8_t slv1_fifo_en	: 1;
		uint8_t slv2_fifo_en	: 1;
		uint8_t accel_fifo_en	: 1;
		uint8_t zg_fifo_en		: 1;
		uint8_t yg_fifo_en		: 1;
		uint8_t xg_fifo_en		: 1;
		uint8_t temp_fifo_en	: 1;	// MSB
	} bit;
} PifMpu60x0FifoEn;


// Register : I2C_MST_CTRL

typedef enum EnPifMpu60x0I2cMstClk
{
    MPU60X0_I2C_MST_CLK_348KHZ,
    MPU60X0_I2C_MST_CLK_333KHZ,
    MPU60X0_I2C_MST_CLK_320KHZ,
    MPU60X0_I2C_MST_CLK_308KHZ,
    MPU60X0_I2C_MST_CLK_296KHZ,
    MPU60X0_I2C_MST_CLK_286KHZ,
    MPU60X0_I2C_MST_CLK_276KHZ,
    MPU60X0_I2C_MST_CLK_267KHZ,
    MPU60X0_I2C_MST_CLK_258KHZ,
    MPU60X0_I2C_MST_CLK_500KHZ,
    MPU60X0_I2C_MST_CLK_471KHZ,
    MPU60X0_I2C_MST_CLK_444KHZ,
    MPU60X0_I2C_MST_CLK_421KHZ,
    MPU60X0_I2C_MST_CLK_400KHZ,
    MPU60X0_I2C_MST_CLK_381KHZ,
    MPU60X0_I2C_MST_CLK_364KHZ
} PifMpu60x0I2cMstClk;

#define MPU60X0_I2C_MST_CTRL_I2C_MST_CLK	0x0004
#define MPU60X0_I2C_MST_CTRL_I2C_MST_P_NSR	0x0401
#define MPU60X0_I2C_MST_CTRL_SLV3_FIFO_EN	0x0501
#define MPU60X0_I2C_MST_CTRL_WAIT_FOR_ES	0x0601
#define MPU60X0_I2C_MST_CTRL_MULT_MST_EN	0x0701

typedef union StPifMpu60x0I2cMstCtrl
{
	uint8_t byte;
	struct {
		PifMpu60x0I2cMstClk i2c_mst_clk		: 4;	// LSB
		uint8_t i2c_mst_p_nsr				: 1;
		uint8_t slv3_fifo_en				: 1;
		uint8_t wait_for_es					: 1;
		uint8_t mult_mst_en					: 1;	// MSB
	} bit;
} PifMpu60x0I2cMstCtrl;


// Register : I2C_SLVx_ADDR

#define MPU60X0_I2C_SLV_ADDR_I2C_SLV_ADDR		0x0007
#define MPU60X0_I2C_SLV_ADDR_I2C_SLV_RW			0x0701

typedef union StPifMpu60x0I2cSlvAddr
{
	uint8_t byte;
	struct {
		uint8_t i2c_slv_addr	: 7;	// LSB
		uint8_t i2c_slv_rw		: 1;	// MSB
	} bit;
} PifMpu60x0I2cSlvAddr;


// Register : I2C_SLVx_CTRL

#define MPU60X0_I2C_SLV_CTRL_I2C_SLV_LEN		0x0004
#define MPU60X0_I2C_SLV_CTRL_I2C_SLV_GRP		0x0401
#define MPU60X0_I2C_SLV_CTRL_I2C_SLV_REG_DIS	0x0501
#define MPU60X0_I2C_SLV_CTRL_I2C_SLV_BYTE_SW	0x0601
#define MPU60X0_I2C_SLV_CTRL_I2C_SLV_EN			0x0701

typedef union StPifMpu60x0I2cSlvCtrl
{
	uint8_t byte;
	struct {
		uint8_t i2c_slv_len			: 4;	// LSB
		uint8_t i2c_slv_grp			: 1;
		uint8_t i2c_slv_reg_dis		: 1;
		uint8_t i2c_slv_byte_sw		: 1;
		uint8_t i2c_slv_en			: 1;	// MSB
	} bit;
} PifMpu60x0I2cSlvCtrl;


// Register : I2C_MST_STSTUS

#define MPU60X0_I2C_MST_STATUS_I2C_SLV0_NACK	0x0001
#define MPU60X0_I2C_MST_STATUS_I2C_SLV1_NACK	0x0101
#define MPU60X0_I2C_MST_STATUS_I2C_SLV2_NACK	0x0201
#define MPU60X0_I2C_MST_STATUS_I2C_SLV3_NACK	0x0301
#define MPU60X0_I2C_MST_STATUS_I2C_SLV4_NACK	0x0401
#define MPU60X0_I2C_MST_STATUS_I2C_LOST_ARB		0x0501
#define MPU60X0_I2C_MST_STATUS_I2C_SLV4_DONE	0x0601
#define MPU60X0_I2C_MST_STATUS_PASS_THROUGH		0x0701

typedef union StPifMpu60x0I2cMstStatus
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
} PifMpu60x0I2cMstStatus;


// Register : INT_PIN_CFG

#define MPU60X0_INT_PIN_CFG_I2C_BYPASS_EN		0x0101
#define MPU60X0_INT_PIN_CFG_FSYNC_INT_EN		0x0201
#define MPU60X0_INT_PIN_CFG_FSYNC_INT_LEVEL		0x0301
#define MPU60X0_INT_PIN_CFG_INT_RD_CLEAR		0x0401
#define MPU60X0_INT_PIN_CFG_LATCH_INT_EN		0x0501
#define MPU60X0_INT_PIN_CFG_INT_OPEN			0x0601
#define MPU60X0_INT_PIN_CFG_INT_LEVEL			0x0701

typedef union StPifMpu60x0IntPinCfg
{
	uint8_t byte;
	struct {
		uint8_t reserved			: 1;	// LSB
		uint8_t i2c_bypass_en		: 1;
		uint8_t fsync_int_en		: 1;
		uint8_t fsync_int_level		: 1;
		uint8_t int_rd_clear		: 1;
		uint8_t latch_int_en		: 1;
		uint8_t int_open			: 1;
		uint8_t int_level			: 1;	// MSB
	} bit;
} PifMpu60x0IntPinCfg;


// Register : INT_ENABLE

#define MPU60X0_INT_ENABLE_DATA_RDY_EN			0x0001
#define MPU60X0_INT_ENABLE_I2C_MST_INT_EN		0x0301
#define MPU60X0_INT_ENABLE_FIFO_OFLOW_EN		0x0401

typedef union StPifMpu60x0IntEnable
{
	uint8_t byte;
	struct {
		uint8_t data_rdy_en		: 1;	// LSB
		uint8_t reserved1		: 2;
		uint8_t i2c_mst_int_en	: 1;
		uint8_t fifo_oflow_en	: 1;
		uint8_t reserved2		: 3;	// MSB
	} bit;
} PifMpu60x0IntEnable;


// Register : INT_STATUS

#define MPU60X0_INT_STATUS_DATA_RDY_INT			0x0001
#define MPU60X0_INT_STATUS_I2C_MST_INT			0x0301
#define MPU60X0_INT_STATUS_FIFO_OFLOW_INT		0x0401

typedef union StPifMpu60x0IntStatus
{
	uint8_t byte;
	struct {
		uint8_t data_rdy_int		: 1;	// LSB
		uint8_t reserved1			: 2;
		uint8_t i2c_mst_int			: 1;
		uint8_t fifo_oflow_int		: 1;
		uint8_t reserved2			: 3;	// MSB
	} bit;
} PifMpu60x0IntStatus;


// Register : I2C_MST_DELAY_CTRL

#define MPU60X0_I2C_MST_DELAY_CTRL_I2C_SLV0_DLY_EN	0x0001
#define MPU60X0_I2C_MST_DELAY_CTRL_I2C_SLV1_DLY_EN	0x0101
#define MPU60X0_I2C_MST_DELAY_CTRL_I2C_SLV2_DLY_EN	0x0201
#define MPU60X0_I2C_MST_DELAY_CTRL_I2C_SLV3_DLY_EN	0x0301
#define MPU60X0_I2C_MST_DELAY_CTRL_I2C_SLV4_DLY_EN	0x0401
#define MPU60X0_I2C_MST_DELAY_CTRL_DELAY_ES_SHADOW	0x0701

typedef union StPifMpu60x0I2cMstDelayCtrl
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
} PifMpu60x0I2cMstDelayCtrl;


// Register : SIGNAL_PATH_RESET

#define MPU60X0_SIGNAL_PATH_RESET_TEMP_RESET		0x0001
#define MPU60X0_SIGNAL_PATH_RESET_ACCEL_RESET		0x0101
#define MPU60X0_SIGNAL_PATH_RESET_GYRO_RESET		0x0201

typedef union StPifMpu60x0SignalPathReset
{
	uint8_t byte;
	struct {
		uint8_t temp_reset		: 1;	// LSB
		uint8_t accel_reset		: 1;
		uint8_t gyro_reset		: 1;
		uint8_t reserved		: 5;	// MSB
	} bit;
} PifMpu60x0SignalPathReset;


// Register : USER_CTRL

#define MPU60X0_USER_CTRL_SIG_COND_RESET	0x0001
#define MPU60X0_USER_CTRL_I2C_MST_RESET		0x0101
#define MPU60X0_USER_CTRL_FIFO_RESET		0x0201
#define MPU60X0_USER_CTRL_I2C_IF_DIS		0x0401
#define MPU60X0_USER_CTRL_I2C_MST_EN		0x0501
#define MPU60X0_USER_CTRL_FIFO_EN			0x0601

typedef union StPifMpu60x0UserCtrl
{
	uint8_t byte;
	struct {
		uint8_t sig_cond_reset		: 1;	// LSB
		uint8_t i2c_mst_reset		: 1;
		uint8_t fifo_reset			: 1;
		uint8_t reserved1			: 1;
		uint8_t i2c_if_dis			: 1;
		uint8_t i2c_mst_en			: 1;
		uint8_t fifo_en				: 1;
		uint8_t reserved2			: 1;	// MSB
	} bit;
} PifMpu60x0UserCtrl;


// Register : PWR_MGMT_1

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

#define MPU60X0_PWR_MGMT_1_CLKSEL			0x0003
#define MPU60X0_PWR_MGMT_1_TEMP_DIS			0x0301
#define MPU60X0_PWR_MGMT_1_CYCLE			0x0501
#define MPU60X0_PWR_MGMT_1_SLEEP			0x0601
#define MPU60X0_PWR_MGMT_1_DEVICE_RESET		0x0701

typedef union StPifMpu60x0PwrMgmt1
{
	uint8_t byte;
	struct {
		PifMpu60x0Clksel clksel		: 3;	// LSB
		uint8_t temp_dis			: 1;
		uint8_t reserved			: 1;
		uint8_t cycle				: 1;
		uint8_t sleep				: 1;
		uint8_t device_reset		: 1;	// MSB
	} bit;
} PifMpu60x0PwrMgmt1;


// Register : PWR_MGMT_2

typedef enum EnPifMpu60x0LpWakeCtrl
{
    MPU60X0_LP_WAKE_CTRL_1_25HZ,
    MPU60X0_LP_WAKE_CTRL_5HZ,
    MPU60X0_LP_WAKE_CTRL_20HZ,
    MPU60X0_LP_WAKE_CTRL_40HZ
} PifMpu60x0LpWakeCtrl;

#define MPU60X0_PWR_MGMT_2_STBY_ZG			0x0001
#define MPU60X0_PWR_MGMT_2_STBY_YG			0x0101
#define MPU60X0_PWR_MGMT_2_STBY_XG			0x0201
#define MPU60X0_PWR_MGMT_2_STBY_ZA			0x0301
#define MPU60X0_PWR_MGMT_2_STBY_YA			0x0401
#define MPU60X0_PWR_MGMT_2_STBY_XA			0x0501
#define MPU60X0_PWR_MGMT_2_LP_WAKE_CTRL		0x0602

typedef union StPifMpu60x0PwrMgmt2
{
	uint8_t byte;
	struct {
		uint8_t stby_zg						: 1;	// LSB
		uint8_t stby_yg						: 1;
		uint8_t stby_xg						: 1;
		uint8_t stby_za						: 1;
		uint8_t stby_ya						: 1;
		uint8_t stby_xa						: 1;
		PifMpu60x0LpWakeCtrl lp_wake_ctrl	: 2;	// MSB
	} bit;
} PifMpu60x0PwrMgmt2;


/**
 * @class StPifMpu60x0Param
 * @brief
 */
typedef struct StPifMpu60x0Param
{
	uint8_t smplrt_div;
	PifMpu60x0DlpfCfg dlpf_cfg;
	PifMpu60x0FsSel fs_sel;
	PifMpu60x0AfsSel afs_sel;
	PifMpu60x0Clksel clksel;
} PifMpu60x0Param;

/**
 * @class StPifMpu60x0
 * @brief
 */
typedef struct StPifMpu60x0
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
} PifMpu60x0;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu60x0_Detect
 * @brief
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifMpu60x0_Detect(PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifMpu60x0_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param p_param
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu60x0_Init(PifMpu60x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, PifMpu60x0Param* p_param, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu60x0_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifMpu60x0_Clear(PifMpu60x0* p_owner);

/**
 * @fn pifMpu60x0_SetGyroConfig
 * @brief
 * @param p_owner
 * @param gyro_config
 * @return
 */
BOOL pifMpu60x0_SetGyroConfig(PifMpu60x0* p_owner, PifMpu60x0GyroConfig gyro_config);

/**
 * @fn pifMpu60x0_SetFsSel
 * @brief
 * @param p_owner
 * @param fs_sel
 * @return
 */
BOOL pifMpu60x0_SetFsSel(PifMpu60x0* p_owner, PifMpu60x0FsSel fs_sel);

/**
 * @fn pifMpu60x0_SetAccelConfig
 * @brief
 * @param p_owner
 * @param accel_config
 * @return
 */
BOOL pifMpu60x0_SetAccelConfig(PifMpu60x0* p_owner, PifMpu60x0AccelConfig accel_config);

/**
 * @fn pifMpu60x0_SetAfsSel
 * @brief
 * @param p_owner
 * @param afs_sel
 * @return
 */
BOOL pifMpu60x0_SetAfsSel(PifMpu60x0* p_owner, PifMpu60x0AfsSel afs_sel);

/**
 * @fn pifMpu60x0_ReadGyro
 * @brief
 * @param p_owner
 * @param p_gyro
 * @return
 */
BOOL pifMpu60x0_ReadGyro(PifMpu60x0* p_owner, int16_t* p_gyro);

/**
 * @fn pifMpu60x0_ReadAccel
 * @brief
 * @param p_owner
 * @param p_accel
 * @return
 */
BOOL pifMpu60x0_ReadAccel(PifMpu60x0* p_owner, int16_t* p_accel);

/**
 * @fn pifMpu60x0_ReadTemperature
 * @brief
 * @param p_owner
 * @param p_temperature
 * @return
 */
BOOL pifMpu60x0_ReadTemperature(PifMpu60x0* p_owner, int16_t* p_temperature);

/**
 * @fn pifMpu60x0_CalibrationGyro
 * @brief
 * @param p_owner
 * @param samples
 * @return
 */
BOOL pifMpu60x0_CalibrationGyro(PifMpu60x0* p_owner, uint8_t samples);

/**
 * @fn pifMpu60x0_SetThreshold
 * @brief
 * @param p_owner
 * @param multiple
 * @return
 */
BOOL pifMpu60x0_SetThreshold(PifMpu60x0* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU60X0_H
