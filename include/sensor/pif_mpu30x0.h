#ifndef PIF_MPU30X0_H
#define PIF_MPU30X0_H


#include "communication/pif_i2c.h"
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

#define MPU30X0_ID(N)				((N) << 1)
#define MPU30X0_I2C_IF_DIS(N)		((N) << 7)

#define MPU30X0_ID_MASK				0x7E
#define MPU30X0_I2C_IF_DIS_MASK		0x80


// Register : PRODUCT_ID

#define MPU30X0_VERSION(N)		(N)
#define MPU30X0_PART_NUM(N)		((N) << 4)

#define MPU30X0_VERSION_MASK	0x0F
#define MPU30X0_PART_NUM_MASK	0xF0


// Register : FIFO_EN

#define MPU30X0_FIFO_FOOTER(N)		(N)
#define MPU30X0_AUX_ZOUT(N)			((N) << 1)
#define MPU30X0_AUX_YOUT(N)			((N) << 2)
#define MPU30X0_AUX_XOUT(N)			((N) << 3)
#define MPU30X0_GYRO_ZOUT(N)		((N) << 4)
#define MPU30X0_GYRO_YOUT(N)		((N) << 5)
#define MPU30X0_GYRO_XOUT(N)		((N) << 6)
#define MPU30X0_TEMP_OUT(N)			((N) << 7)

#define MPU30X0_FIFO_FOOTER_MASK	0x01
#define MPU30X0_AUX_ZOUT_MASK		0x02
#define MPU30X0_AUX_YOUT_MASK		0x04
#define MPU30X0_AUX_XOUT_MASK		0x08
#define MPU30X0_GYRO_ZOUT_MASK		0x10
#define MPU30X0_GYRO_YOUT_MASK		0x20
#define MPU30X0_GYRO_XOUT_MASK		0x40
#define MPU30X0_TEMP_OUT_MASK		0x80


// Register : AUX_VDDIO

#define MPU30X0_AUX_VDDIO(N)	((N) << 2)

#define MPU30X0_AUX_VDDIO_MASK	0x04


// Register : AUX_SLV_ADDR

#define MPU30X0_AUX_ID(N)		(N)
#define MPU30X0_CLKOUT_EN(N)	((N) << 7)

#define MPU30X0_AUX_ID_MASK		0x7F
#define MPU30X0_CLKOUT_EN_MASK	0x80


// Register : DLPF_FS_SYNC

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

typedef enum EnPifMpu30x0FsSel
{
    MPU30X0_FS_SEL_250DPS				= 0 << 3,
    MPU30X0_FS_SEL_500DPS				= 1 << 3,
    MPU30X0_FS_SEL_1000DPS				= 2 << 3,
    MPU30X0_FS_SEL_2000DPS				= 3 << 3
} PifMpu30x0FsSel;

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

#define MPU30X0_DLPF_CFG_MASK			0x07
#define MPU30X0_FS_SEL_MASK				0x18
#define MPU30X0_EXT_SYNC_SET_MASK		0xE0


// Register : INT_CFG

#define MPU30X0_RAW_RDY_EN(N)			(N)
#define MPU30X0_DMP_DONE_EN(N)			((N) << 1)
#define MPU30X0_MPU_RDY_EN(N)			((N) << 2)
#define MPU30X0_INT_ANYRD_2CLEAR(N)		((N) << 4)
#define MPU30X0_LATCH_INT_EN(N)			((N) << 5)
#define MPU30X0_OPEN(N)					((N) << 6)
#define MPU30X0_ACTL(N)					((N) << 7)

#define MPU30X0_RAW_RDY_EN_MASK			0x01
#define MPU30X0_DMP_DONE_EN_MASK		0x02
#define MPU30X0_MPU_RDY_EN_MASK			0x04
#define MPU30X0_INT_ANYRD_2CLEAR_MASK	0x10
#define MPU30X0_LATCH_INT_EN_MASK		0x20
#define MPU30X0_OPEN_MASK				0x40
#define MPU30X0_ACTL_MASK				0x80


// Register : INT_STATUS

#define MPU30X0_RAW_DATA_RDY(N)		(N)
#define MPU30X0_DMP_DONE(N)			((N) << 1)
#define MPU30X0_MPU_RDY(N)			((N) << 2)

#define MPU30X0_RAW_DATA_RDY_MASK	0x01
#define MPU30X0_DMP_DONE_MASK		0x02
#define MPU30X0_MPU_RDY_MASK		0x04


// Register : USER_CTRL

#define MPU30X0_GYRO_RST(N)			(N)
#define MPU30X0_FIFO_RST(N)			((N) << 1)
#define MPU30X0_AUX_IF_RST(N)		((N) << 3)
#define MPU30X0_AUX_IF_EN(N)		((N) << 5)
#define MPU30X0_FIFO_EN(N)			((N) << 6)

#define MPU30X0_GYRO_RST_MASK		0x01
#define MPU30X0_FIFO_RST_MASK		0x02
#define MPU30X0_AUX_IF_RST_MASK		0x08
#define MPU30X0_AUX_IF_EN_MASK		0x20
#define MPU30X0_FIFO_EN_MASK		0x40


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

#define MPU30X0_STBY_ZG(N)					((N) << 3)
#define MPU30X0_STBY_YG(N)					((N) << 4)
#define MPU30X0_STBY_XG(N)					((N) << 5)
#define MPU30X0_SLEEP(N)					((N) << 6)
#define MPU30X0_H_RESET(N)					((N) << 7)

#define MPU30X0_CLK_SEL_MASK				0x07
#define MPU30X0_STBY_ZG_MASK				0x08
#define MPU30X0_STBY_YG_MASK				0x10
#define MPU30X0_STBY_XG_MASK				0x20
#define MPU30X0_SLEEP_MASK					0x40
#define MPU30X0_H_RESET_MASK				0x80


/**
 * @class StPifMpu30x0
 * @brief Defines the st pif mpu30x0 data structure.
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
 * @brief Performs the mpu30x0 detect operation.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifMpu30x0_Init
 * @brief Initializes mpu30x0 init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_Init(PifMpu30x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu30x0_Clear
 * @brief Releases resources used by mpu30x0 clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifMpu30x0_Clear(PifMpu30x0* p_owner);

/**
 * @fn pifMpu30x0_SetDlpfFsSync
 * @brief Sets configuration values required by mpu30x0 set dlpf fs sync.
 * @param p_owner Pointer to the owner instance.
 * @param dlpf_fs_sync Parameter dlpf_fs_sync used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_SetDlpfFsSync(PifMpu30x0* p_owner, uint8_t dlpf_fs_sync);

/**
 * @fn pifMpu30x0_SetFsSel
 * @brief Sets configuration values required by mpu30x0 set fs sel.
 * @param p_owner Pointer to the owner instance.
 * @param fs_sel Gyroscope full-scale range selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_SetFsSel(PifMpu30x0* p_owner, PifMpu30x0FsSel fs_sel);

/**
 * @fn pifMpu30x0_ReadGyro
 * @brief Reads raw data from mpu30x0 read gyro.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Pointer to gyro.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_ReadGyro(PifMpu30x0* p_owner, int16_t* p_gyro);

/**
 * @fn pifMpu30x0_ReadTemperature
 * @brief Reads raw data from mpu30x0 read temperature.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Pointer to temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_ReadTemperature(PifMpu30x0* p_owner, float* p_temperature);

/**
 * @fn pifMpu30x0_CalibrationGyro
 * @brief Performs the mpu30x0 calibration gyro operation.
 * @param p_owner Pointer to the owner instance.
 * @param samples Parameter samples used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_CalibrationGyro(PifMpu30x0* p_owner, uint8_t samples);

/**
 * @fn pifMpu30x0_SetThreshold
 * @brief Sets configuration values required by mpu30x0 set threshold.
 * @param p_owner Pointer to the owner instance.
 * @param multiple Parameter multiple used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu30x0_SetThreshold(PifMpu30x0* p_owner, uint8_t multiple);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU30X0_H
