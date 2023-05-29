#ifndef PIF_QMC5883_H
#define PIF_QMC5883_H


#include "core/pif_i2c.h"
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

#define QMC5883_STATUS_DRDY		0x0001
#define QMC5883_STATUS_OVL		0x0101
#define QMC5883_STATUS_DOR		0x0201

typedef union StPifQmc5883Status
{
	uint8_t byte;
	struct {
		uint8_t drdy		: 1;	// LSB
		uint8_t ovl			: 1;
		uint8_t dor			: 1;
		uint8_t reserved	: 5;	// MSB
	} bit;
} PifQmc5883Status;


// Register : CONTROL_1

typedef enum EnPifQmc5883Mode
{
    QMC5883_MODE_STANDBY,
    QMC5883_MODE_CONTIMUOUS
} PifQmc5883Mode;

typedef enum EnPifQmc5883Odr
{
    QMC5883_ODR_10HZ,
    QMC5883_ODR_50HZ,
    QMC5883_ODR_100HZ,
    QMC5883_ODR_200HZ
} PifQmc5883Odr;

typedef enum EnPifQmc5883Rng
{
    QMC5883_RNG_2G,
    QMC5883_RNG_8G
} PifQmc5883Rng;

typedef enum EnPifQmc5883Osr
{
    QMC5883_OSR_512,
    QMC5883_OSR_256,
    QMC5883_OSR_128,
    QMC5883_OSR_64
} PifQmc5883Osr;

#define QMC5883_CONTROL_1_MODE		0x0002
#define QMC5883_CONTROL_1_ODR		0x0202
#define QMC5883_CONTROL_1_RNG		0x0402	// Use pifQmc5883_SetControl1 to change this value.
#define QMC5883_CONTROL_1_OSR		0x0602

typedef union StPifQmc5883Control1
{
	uint8_t byte;
	struct {
		PifQmc5883Mode mode	: 2;	// LSB
		PifQmc5883Odr odr	: 2;	
		PifQmc5883Rng rng	: 2;	// Use pifQmc5883_SetControl1 to change this value.
		PifQmc5883Osr osr	: 2;	// MSB
	} bit;
} PifQmc5883Control1;


// Register : CONTROL_2

#define QMC5883_CONTROL_2_INT_ENB		0x0001
#define QMC5883_CONTROL_2_ROL_PNT		0x0601
#define QMC5883_CONTROL_2_SOFT_RST		0x0701

typedef union StPifQmc5883Control2
{
	uint8_t byte;
	struct {
		uint8_t int_enb		: 1;	// LSB
		uint8_t reserved	: 5;
		uint8_t rol_pnt		: 1;
		uint8_t soft_rst	: 1;	// MSB
	} bit;
} PifQmc5883Control2;


/**
 * @class StPifQmc5883
 * @brief
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
 * @brief
 * @param p_i2c
 * @return
 */
BOOL pifQmc5883_Detect(PifI2cPort* p_i2c);

/**
 * @fn pifQmc5883_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param p_imu_sensor
 * @return
 */
BOOL pifQmc5883_Init(PifQmc5883* p_owner, PifId id, PifI2cPort* p_i2c, PifImuSensor* p_imu_sensor);

/**
 * @fn pifQmc5883_Clear
 * @brief
 * @param p_owner
 */
void pifQmc5883_Clear(PifQmc5883* p_owner);

/**
 * @fn pifQmc5883_SetControl1
 * @brief
 * @param p_owner
 * @param contorl_1
 * @return
 */
BOOL pifQmc5883_SetControl1(PifQmc5883* p_owner, PifQmc5883Control1 contorl_1);

/**
 * @fn pifQmc5883_ReadMag
 * @brief
 * @param p_owner
 * @param p_mag
 * @return
 */
BOOL pifQmc5883_ReadMag(PifQmc5883* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_QMC5883_H
