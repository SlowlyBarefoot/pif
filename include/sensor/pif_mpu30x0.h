#ifndef PIF_MPU30X0_H
#define PIF_MPU30X0_H


#include "core/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define MPU30X0_I2C_ADDR			0x68


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

#define MPU30X0_WHO_AM_I_ID					0x0106
#define MPU30X0_WHO_AM_I_I2C_IF_DIS			0x0701

typedef union StPifMpu30x0WhoAmI
{
	uint8_t byte;
	struct {
		uint8_t reserved			: 1;	// LSB
		uint8_t id					: 6;
		uint8_t i2c_if_dis			: 1;	// MSB
	} bit;
} PifMpu30x0WhoAmI;


// Register : PRODUCT_ID

#define MPU30X0_PRODUCT_ID_VERSION			0x0004
#define MPU30X0_PRODUCT_ID_PART_NUM			0x0404

typedef union StPifMpu30x0ProductId
{
	uint8_t byte;
	struct {
		uint8_t verison				: 4;	// LSB
		uint8_t part_num			: 4;	// MSB
	} bit;
} PifMpu30x0ProductId;


// Register : FIFO_EN

#define MPU30X0_FIFO_EN_FIFO_FOOTER			0x0001
#define MPU30X0_FIFO_EN_AUX_ZOUT			0x0101
#define MPU30X0_FIFO_EN_AUX_YOUT			0x0201
#define MPU30X0_FIFO_EN_AUX_XOUT			0x0301
#define MPU30X0_FIFO_EN_GYRO_ZOUT			0x0401
#define MPU30X0_FIFO_EN_GYRO_YOUT			0x0501
#define MPU30X0_FIFO_EN_GYRO_XOUT			0x0601
#define MPU30X0_FIFO_EN_TEMP_OUT			0x0701

typedef union StPifMpu30x0FifoEn
{
	uint8_t byte;
	struct {
		uint8_t fifo_footer			: 1;	// LSB
		uint8_t aux_zout			: 1;
		uint8_t aux_yout			: 1;
		uint8_t aux_xout			: 1;
		uint8_t gyro_zout			: 1;
		uint8_t gyro_yout			: 1;
		uint8_t gyro_xout			: 1;
		uint8_t temp_out			: 1;	// MSB
	} bit;
} PifMpu30x0FifoEn;


// Register : AUX_VDDIO

#define MPU30X0_AUX_VDDIO_AUX_VDDIO			0x0201

typedef union StPifMpu30x0AuxVddio
{
	uint8_t byte;
	struct {
		uint8_t reserved1			: 2;	// LSB
		uint8_t aux_vddio			: 1;
		uint8_t reserved2			: 5;	// MSB
	} bit;
} PifMpu30x0AuxVddio;


// Register : AUX_SLV_ADDR

#define MPU30X0_AUX_SLV_ADDR_AUX_ID			0x0007
#define MPU30X0_AUX_SLV_ADDR_CLKOUT_EN		0x0701

typedef union StPifMpu30x0AuxSlvAddr
{
	uint8_t byte;
	struct {
		uint8_t aux_id				: 7;	// LSB
		uint8_t clkout_en			: 1;	// MSB
	} bit;
} PifMpu30x0AuxSlvAddr;


// Register : DLPF_FS_SYNC

typedef enum EnPifMpu30x0DlpfCfg
{
	MPU30X0_DLPF_CFG_256HZ,
	MPU30X0_DLPF_CFG_188HZ,
	MPU30X0_DLPF_CFG_98HZ,
	MPU30X0_DLPF_CFG_42HZ,
	MPU30X0_DLPF_CFG_20HZ,
	MPU30X0_DLPF_CFG_10HZ,
    MPU30X0_DLPF_CFG_5HZ
} PifMpu30x0DlpfCfg;

typedef enum EnPifMpu30x0FsSel
{
    MPU30X0_FS_SEL_250DPS,
    MPU30X0_FS_SEL_500DPS,
    MPU30X0_FS_SEL_1000DPS,
    MPU30X0_FS_SEL_2000DPS
} PifMpu30x0FsSel;

typedef enum EnPifMpu30x0ExtSyncSet
{
	MPU30X0_EXT_SYNC_SET_NO_SYNC,
	MPU30X0_EXT_SYNC_SET_TEMP_OUT_L,
	MPU30X0_EXT_SYNC_SET_GYRO_XOUT_L,
	MPU30X0_EXT_SYNC_SET_GYRO_YOUT_L,
	MPU30X0_EXT_SYNC_SET_GYRO_ZOUT_L,
	MPU30X0_EXT_SYNC_SET_AUX_XOUT_L,
    MPU30X0_EXT_SYNC_SET_AUX_YOUT_L,
    MPU30X0_EXT_SYNC_SET_AUX_ZOUT_L
} PifMpu30x0ExtSyncSet;

#define MPU30X0_DLPF_FS_SYNC_DLPF_CFG		0x0003
#define MPU30X0_DLPF_FS_SYNC_FS_SEL			0x0302
#define MPU30X0_DLPF_FS_SYNC_EXT_SYNC_SET	0x0503

typedef union StPifMpu30x0DlpfFsSync
{
	uint8_t byte;
	struct {
		PifMpu30x0DlpfCfg dlpf_cfg			: 3;	// LSB
		PifMpu30x0FsSel fs_sel				: 2;
		PifMpu30x0ExtSyncSet ext_sync_set	: 3;	// MSB
	} bit;
} PifMpu30x0DlpfFsSync;


// Register : INT_CFG

#define MPU30X0_INT_CFG_RAW_RDY_EN			0x0001
#define MPU30X0_INT_CFG_DMP_DONE_EN			0x0101
#define MPU30X0_INT_CFG_MPU_RDY_EN			0x0201
#define MPU30X0_INT_CFG_INT_ANYRD_2CLEAR	0x0401
#define MPU30X0_INT_CFG_LATCH_INT_EN		0x0501
#define MPU30X0_INT_CFG_OPEN				0x0601
#define MPU30X0_INT_CFG_ACTL				0x0701

typedef union StPifMpu30x0IntCfg
{
	uint8_t byte;
	struct {
		uint8_t raw_rdy_en			: 1;	// LSB
		uint8_t dmp_done_en			: 1;
		uint8_t mpu_rdy_en			: 1;
		uint8_t reserved			: 1;
		uint8_t int_anyrd_2clear	: 1;
		uint8_t latch_int_en		: 1;
		uint8_t open				: 1;
		uint8_t actl				: 1;	// MSB
	} bit;
} PifMpu30x0IntCfg;


// Register : INT_STATUS

#define MPU30X0_INT_STATUS_RAW_DATA_RDY		0x0001
#define MPU30X0_INT_STATUS_DMP_DONE			0x0101
#define MPU30X0_INT_STATUS_MPU_RDY			0x0201

typedef union StPifMpu30x0IntStatus
{
	uint8_t byte;
	struct {
		uint8_t raw_data_rdy		: 1;	// LSB
		uint8_t dmp_done			: 1;
		uint8_t mpu_rdy				: 1;	// MSB
	} bit;
} PifMpu30x0IntStatus;


// Register : USER_CTRL

#define MPU30X0_USER_CTRL_GYRO_RST			0x0001
#define MPU30X0_USER_CTRL_FIFO_RST			0x0101
#define MPU30X0_USER_CTRL_AUX_IF_RST		0x0301
#define MPU30X0_USER_CTRL_AUX_IF_EN			0x0501
#define MPU30X0_USER_CTRL_FIFO_EN			0x0601

typedef union StPifMpu30x0UserCtrl
{
	uint8_t byte;
	struct {
		uint8_t gyro_rst			: 1;	// LSB
		uint8_t fifo_rst			: 1;
		uint8_t reserved1			: 1;
		uint8_t aux_if_rst			: 1;
		uint8_t reserved2			: 1;
		uint8_t aux_if_en			: 1;
		uint8_t fifo_en				: 1;
		uint8_t reserved3			: 1;	// MSB
	} bit;
} PifMpu30x0UserCtrl;


// Register : PWR_MGMT

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

#define MPU30X0_PWR_MGMT_CLK_SEL			0x0003
#define MPU30X0_PWR_MGMT_STBY_ZG			0x0301
#define MPU30X0_PWR_MGMT_STBY_YG			0x0401
#define MPU30X0_PWR_MGMT_STBY_XG			0x0501
#define MPU30X0_PWR_MGMT_SLEEP				0x0601
#define MPU30X0_PWR_MGMT_H_RESET			0x0701

typedef union StPifMpu30x0PwrMgmt
{
	uint8_t byte;
	struct {
		PifMpu30x0ClkSel clk_sel	: 3;	// LSB
		uint8_t stby_zg				: 1;
		uint8_t stby_yg				: 1;
		uint8_t stby_xg				: 1;
		uint8_t sleep				: 1;
		uint8_t h_reset				: 1;	// MSB
	} bit;
} PifMpu30x0PwrMgmt;


/**
 * @class StPifMpu30x0
 * @brief
 */
typedef struct StPifMpu30x0
{
	// Public Member Variable
	uint8_t scale;

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifMpu30x0;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu30x0_Detect
 * @brief
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifMpu30x0_Detect(PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifMpu30x0_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu30x0_Init(PifMpu30x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu30x0_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifMpu30x0_Clear(PifMpu30x0* p_owner);

/**
 * @fn pifMpu30x0_SetDlpfFsSync
 * @brief
 * @param p_owner
 * @param dlpf_fs_sync
 * @return
 */
BOOL pifMpu30x0_SetDlpfFsSync(PifMpu30x0* p_owner, PifMpu30x0DlpfFsSync dlpf_fs_sync);

/**
 * @fn pifMpu30x0_SetFsSel
 * @brief
 * @param p_owner
 * @param fs_sel
 * @return
 */
BOOL pifMpu30x0_SetFsSel(PifMpu30x0* p_owner, PifMpu30x0FsSel fs_sel);

/**
 * @fn pifMpu30x0_ReadGyro
 * @brief
 * @param p_owner
 * @param p_gyro
 * @return
 */
BOOL pifMpu30x0_ReadGyro(PifMpu30x0* p_owner, int16_t* p_gyro);

/**
 * @fn pifMpu30x0_ReadTemperature
 * @brief
 * @param p_owner
 * @param p_temperature
 * @return
 */
BOOL pifMpu30x0_ReadTemperature(PifMpu30x0* p_owner, float* p_temperature);

/**
 * @fn pifMpu30x0_CalibrationGyro
 * @brief
 * @param p_owner
 * @param samples
 * @return
 */
BOOL pifMpu30x0_CalibrationGyro(PifMpu30x0* p_owner, uint8_t samples);

/**
 * @fn pifMpu30x0_SetThreshold
 * @brief
 * @param p_owner
 * @param multiple
 * @return
 */
BOOL pifMpu30x0_SetThreshold(PifMpu30x0* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU30X0_H
