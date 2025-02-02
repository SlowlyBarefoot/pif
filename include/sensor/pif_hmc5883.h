#ifndef PIF_HMC5883_H
#define PIF_HMC5883_H


#include "communication/pif_i2c.h"
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
    HMC5883_MEASURE_MODE_NORMAL		= 0,		// Default
    HMC5883_MEASURE_MODE_POS_BIAS	= 1,
    HMC5883_MEASURE_MODE_NEG_BIAS	= 2,

    HMC5883_MEASURE_MODE_DEFAULT	= 0
} PifHmc5883MeasureMode;

typedef enum EnPifHmc5883DataRate
{
    HMC5883_DATARATE_0_75_HZ		= 0 << 2,
    HMC5883_DATARATE_1_5HZ			= 1 << 2,
    HMC5883_DATARATE_3HZ			= 2 << 2,
    HMC5883_DATARATE_7_5HZ			= 3 << 2,
    HMC5883_DATARATE_15HZ			= 4 << 2,	// Default
    HMC5883_DATARATE_30HZ			= 5 << 2,
    HMC5883_DATARATE_75HZ			= 6 << 2,

	HMC5883_DATARATE_DEFAULT		= 4 << 2
} PifHmc5883DataRate;

typedef enum EnPifHmc5883Samples
{
    HMC5883_SAMPLES_1				= 0 << 5,	// Default
    HMC5883_SAMPLES_2				= 1 << 5,
    HMC5883_SAMPLES_4				= 2 << 5,
    HMC5883_SAMPLES_8				= 3 << 5,

	HMC5883_SAMPLES_DEFAULT			= 0 << 5
} PifHmc5883Samples;

#define HMC5883_MEASURE_MODE_MASK	0x03
#define HMC5883_DATA_RATE_MASK		0x1C
#define HMC5883_SAMPLES_MASK		0x60


// Register : CONFIG_B

typedef enum EnPifHmc5883Gain
{
    HMC5883_GAIN_0_88GA		= 0 << 5,
    HMC5883_GAIN_1_3GA		= 1 << 5,	// Default
    HMC5883_GAIN_1_9GA		= 2 << 5,
	HMC5883_GAIN_2_5GA		= 3 << 5,
	HMC5883_GAIN_4GA		= 4 << 5,
	HMC5883_GAIN_4_7GA		= 5 << 5,
	HMC5883_GAIN_5_6GA		= 6 << 5,
	HMC5883_GAIN_8_1GA		= 7 << 5,

	HMC5883_GAIN_DEFAULT	= 1 << 5
} PifHmc5883Gain;

#define HMC5883_GAIN_MASK	0xE0		// Use pifHmc5883_SetGain to change this value.


// Register : MODE

typedef enum EnPifHmc5883Mode
{
    HMC5883_MODE_CONTINOUS			= 0,
    HMC5883_MODE_SINGLE				= 1,	// Default
    HMC5883_MODE_IDLE				= 2,

    HMC5883_MODE_DEFAULT			= 1
} PifHmc5883Mode;

#define HMC5883_HIGH_SPEED(N)		((N) << 7)

#define HMC5883_MODE_MASK			0x03
#define HMC5883_HIGH_SPEED_MASK		0x80


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
 * @param p_client
 * @param p_imu_sensor
 * @return
 */
BOOL pifHmc5883_Init(PifHmc5883* p_owner, PifId id, PifI2cPort* p_i2c, void *p_client, PifImuSensor* p_imu_sensor);

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
