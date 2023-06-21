#ifndef PIF_HMC5883_H
#define PIF_HMC5883_H


#include "core/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define HMC5883_I2C_ADDR			0x1E


typedef enum EnPifHmc5883Reg
{
	HMC5883_REG_CONFIG_A			= 0x00,
	HMC5883_REG_CONFIG_B			= 0x01,
	HMC5883_REG_MODE				= 0x02,
	HMC5883_REG_OUT_X_M				= 0x03,
	HMC5883_REG_OUT_X_L				= 0x04,
	HMC5883_REG_OUT_Z_M				= 0x05,
	HMC5883_REG_OUT_Z_L				= 0x06,
	HMC5883_REG_OUT_Y_M				= 0x07,
	HMC5883_REG_OUT_Y_L				= 0x08,
	HMC5883_REG_STATUS				= 0x09,
	HMC5883_REG_IDENT_A				= 0x0A,
	HMC5883_REG_IDENT_B				= 0x0B,
	HMC5883_REG_IDENT_C				= 0x0C,
} PifHmc5883Reg;


// Register : CONFIG_A

typedef enum EnPifHmc5883MeasureMode
{
    HMC5883_MEASURE_MODE_NORMAL,		// Default
    HMC5883_MEASURE_MODE_POS_BIAS,
    HMC5883_MEASURE_MODE_NEG_BIAS,

    HMC5883_MEASURE_MODE_DEFAULT	= 0
} PifHmc5883MeasureMode;

typedef enum EnPifHmc5883DataRate
{
    HMC5883_DATARATE_0_75_HZ,
    HMC5883_DATARATE_1_5HZ,
    HMC5883_DATARATE_3HZ,
    HMC5883_DATARATE_7_5HZ,
    HMC5883_DATARATE_15HZ,				// Default
    HMC5883_DATARATE_30HZ,
    HMC5883_DATARATE_75HZ,

	HMC5883_DATARATE_DEFAULT	= 4
} PifHmc5883DataRate;

typedef enum EnPifHmc5883Samples
{
    HMC5883_SAMPLES_1,					// Default
    HMC5883_SAMPLES_2,
    HMC5883_SAMPLES_4,
    HMC5883_SAMPLES_8,

	HMC5883_SAMPLES_DEFAULT	= 0
} PifHmc5883Samples;

#define HMC5883_CONFIG_A_MEASURE_MODE		0x0002
#define HMC5883_CONFIG_A_DATA_RATE			0x0203
#define HMC5883_CONFIG_A_SAMPLES			0x0502

typedef union StPifHmc5883ConfigA
{
	uint8_t byte;
	struct {
		PifHmc5883MeasureMode measure_mode	: 2;	// LSB
		PifHmc5883DataRate data_rate		: 3;
		PifHmc5883Samples samples			: 2;
		uint8_t reserved					: 1;	// MSB
	} bit;
} PifHmc5883ConfigA;


// Register : CONFIG_B

typedef enum EnPifHmc5883Gain
{
    HMC5883_GAIN_0_88GA,
    HMC5883_GAIN_1_3GA,				// Default
    HMC5883_GAIN_1_9GA,
	HMC5883_GAIN_2_5GA,
	HMC5883_GAIN_4GA,
	HMC5883_GAIN_4_7GA,
	HMC5883_GAIN_5_6GA,
	HMC5883_GAIN_8_1GA,

	HMC5883_GAIN_DEFAULT	= 1
} PifHmc5883Gain;

#define HMC5883_CONFIG_B_GAIN		0x0503		// Use pifHmc5883_SetGain to change this value.

typedef union StPifHmc5883ConfigB
{
	uint8_t byte;
	struct {
		uint8_t reserved	: 5;	// LSB
		PifHmc5883Gain gain	: 3;	// MSB : Use pifHmc5883_SetGain to change this value.
	} bit;
} PifHmc5883ConfigB;


// Register : MODE

typedef enum EnPifHmc5883Mode
{
    HMC5883_MODE_CONTINOUS,
    HMC5883_MODE_SINGLE,			// Default
    HMC5883_MODE_IDLE,

    HMC5883_MODE_DEFAULT	= 1
} PifHmc5883Mode;

#define HMC5883_MODE_MODE			0x0002
#define HMC5883_MODE_HIGH_SPEED		0x0701

typedef union StPifHmc5883ModeReg
{
	uint8_t byte;
	struct {
		PifHmc5883Mode mode	: 2;	// LSB
		uint8_t reserved	: 5;
		BOOL high_speed		: 1;	// MSB
	} bit;
} PifHmc5883ModeReg;


/**
 * @class StPifHmc5883
 * @brief
 */
typedef struct StPifHmc5883
{
	// Public Member Variable
	float scale[3];

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifHmc5883;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifHmc5883_Detect
 * @brief
 * @param p_i2c
 * @return
 */
BOOL pifHmc5883_Detect(PifI2cPort* p_i2c);

/**
 * @fn pifHmc5883_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param p_imu_sensor
 * @return
 */
BOOL pifHmc5883_Init(PifHmc5883* p_owner, PifId id, PifI2cPort* p_i2c, PifImuSensor* p_imu_sensor);

/**
 * @fn pifHmc5883_Clear
 * @brief
 * @param p_owner
 */
void pifHmc5883_Clear(PifHmc5883* p_owner);

/**
 * @fn pifHmc5883_SetGain
 * @brief
 * @param p_owner
 * @param gain
 * @return
 */
BOOL pifHmc5883_SetGain(PifHmc5883* p_owner, PifHmc5883Gain gain);

/**
 * @fn pifHmc5883_ReadMag
 * @brief
 * @param p_owner
 * @param p_mag
 * @return
 */
BOOL pifHmc5883_ReadMag(PifHmc5883* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_HMC5883_H
