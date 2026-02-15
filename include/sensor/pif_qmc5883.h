#ifndef PIF_QMC5883_H
#define PIF_QMC5883_H


#include "communication/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define QMC5883_I2C_ADDR			0x0D


typedef enum EnPifQmc5883Reg
{
	QMC5883_REG_OUT_X_LSB			= 0x00,
	QMC5883_REG_OUT_X_MSB			= 0x01,
	QMC5883_REG_OUT_Y_LSB			= 0x02,
	QMC5883_REG_OUT_Y_MSB			= 0x03,
	QMC5883_REG_OUT_Z_LSB			= 0x04,
	QMC5883_REG_OUT_Z_MSB			= 0x05,
	QMC5883_REG_STATUS				= 0x06,
	QMC5883_REG_TEMP_LSB			= 0x07,
	QMC5883_REG_TEMP_MSB			= 0x08,
	QMC5883_REG_CONTROL_1			= 0x09,
	QMC5883_REG_CONTROL_2			= 0x0A,
	QMC5883_REG_SET_RESET_PERIOD	= 0x0B,
	QMC5883_REG_CHIP_ID				= 0x0D,
} PifQmc5883Reg;


// Register : STATUS

#define QMC5883_DRDY(N)		(N)
#define QMC5883_OVL(N)		((N) << 1)
#define QMC5883_DOR(N)		((N) << 2)

#define QMC5883_DRDY_MASK	0x01
#define QMC5883_OVL_MASK	0x02
#define QMC5883_DOR_MASK	0x04


// Register : CONTROL_1

typedef enum EnPifQmc5883Mode
{
    QMC5883_MODE_STANDBY	= 0,
    QMC5883_MODE_CONTIMUOUS	= 1
} PifQmc5883Mode;

typedef enum EnPifQmc5883Odr
{
    QMC5883_ODR_10HZ		= 0 << 2,
    QMC5883_ODR_50HZ		= 1 << 2,
    QMC5883_ODR_100HZ		= 2 << 2,
    QMC5883_ODR_200HZ		= 3 << 2
} PifQmc5883Odr;

typedef enum EnPifQmc5883Rng
{
    QMC5883_RNG_2G			= 0 << 4,
    QMC5883_RNG_8G			= 1 << 4
} PifQmc5883Rng;

typedef enum EnPifQmc5883Osr
{
    QMC5883_OSR_512			= 0 << 6,
    QMC5883_OSR_256			= 1 << 6,
    QMC5883_OSR_128			= 2 << 6,
    QMC5883_OSR_64			= 3 << 6
} PifQmc5883Osr;

#define QMC5883_MODE_MASK	0x03
#define QMC5883_ODR_MASK	0x0C
#define QMC5883_RNG_MASK	0x30		// Use pifQmc5883_SetControl1 to change this value.
#define QMC5883_OSR_MASK	0xC0


// Register : CONTROL_2

#define QMC5883_INT_ENB(N)		(N)
#define QMC5883_ROL_PNT(N)		((N) << 6)
#define QMC5883_SOFT_RST(N)		((N) << 7)

#define QMC5883_INT_ENB_MASK	0x01
#define QMC5883_ROL_PNT_MASK	0x40
#define QMC5883_SOFT_RST_MASK	0x80


/**
 * @class StPifQmc5883
 * @brief Defines the st pif qmc5883 data structure.
 */
typedef struct StPifQmc5883
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifQmc5883;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifQmc5883_Detect
 * @brief Performs the qmc5883 detect operation.
 * @param p_i2c Pointer to i2c.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_Detect(PifI2cPort* p_i2c, void *p_client);

/**
 * @fn pifQmc5883_Init
 * @brief Initializes qmc5883 init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_Init(PifQmc5883* p_owner, PifId id, PifI2cPort* p_i2c, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @fn pifQmc5883_Clear
 * @brief Releases resources used by qmc5883 clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifQmc5883_Clear(PifQmc5883* p_owner);

/**
 * @fn pifQmc5883_SetControl1
 * @brief Sets configuration values required by qmc5883 set control1.
 * @param p_owner Pointer to the owner instance.
 * @param contorl_1 Parameter contorl_1 used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_SetControl1(PifQmc5883* p_owner, uint8_t contorl_1);

/**
 * @fn pifQmc5883_ReadMag
 * @brief Reads raw data from qmc5883 read mag.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Pointer to mag.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_ReadMag(PifQmc5883* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_QMC5883_H
