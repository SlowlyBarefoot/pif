// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_qmc5883.h
 * @brief QST QMC5883L 3-axis magnetometer on I2C, as the magnetometer of a PifImuSensor.
 */
#ifndef PIF_QMC5883_H
#define PIF_QMC5883_H


#include "communication/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define QMC5883_I2C_ADDR			0x0D	///< I2C address


/**
 * @brief Register addresses.
 */
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

/** @name QMC5883 STATUS register
 *  @{
 */
#define QMC5883_DRDY(N)		(N)	///< Puts N into the DRDY field
#define QMC5883_OVL(N)		((N) << 1)	///< Puts N into the OVL field
#define QMC5883_DOR(N)		((N) << 2)	///< Puts N into the DOR field

#define QMC5883_DRDY_MASK	0x01	///< Mask of the DRDY field
#define QMC5883_OVL_MASK	0x02	///< Mask of the OVL field
#define QMC5883_DOR_MASK	0x04	///< Mask of the DOR field
/** @} */


// Register : CONTROL_1

/** @name QMC5883 CONTROL_1 register
 *  @{
 */
/**
 * @brief Bits 1-0 (MODE): standby or continuous measurement.
 */
typedef enum EnPifQmc5883Mode
{
    QMC5883_MODE_STANDBY	= 0,
    QMC5883_MODE_CONTIMUOUS	= 1
} PifQmc5883Mode;

/**
 * @brief Bits 3-2 (ODR): output data rate.
 */
typedef enum EnPifQmc5883Odr
{
    QMC5883_ODR_10HZ		= 0 << 2,
    QMC5883_ODR_50HZ		= 1 << 2,
    QMC5883_ODR_100HZ		= 2 << 2,
    QMC5883_ODR_200HZ		= 3 << 2
} PifQmc5883Odr;

/**
 * @brief Bits 5-4 (RNG): full scale range in Gauss.
 */
typedef enum EnPifQmc5883Rng
{
    QMC5883_RNG_2G			= 0 << 4,
    QMC5883_RNG_8G			= 1 << 4
} PifQmc5883Rng;

/**
 * @brief Bits 7-6 (OSR): over sample ratio.
 */
typedef enum EnPifQmc5883Osr
{
    QMC5883_OSR_512			= 0 << 6,
    QMC5883_OSR_256			= 1 << 6,
    QMC5883_OSR_128			= 2 << 6,
    QMC5883_OSR_64			= 3 << 6
} PifQmc5883Osr;

#define QMC5883_MODE_MASK	0x03		///< Mask of the MODE field
#define QMC5883_ODR_MASK	0x0C		///< Mask of the ODR field
#define QMC5883_RNG_MASK	0x30		///< Use pifQmc5883_SetControl1 to change this value.
#define QMC5883_OSR_MASK	0xC0		///< Mask of the OSR field
/** @} */


// Register : CONTROL_2

/** @name QMC5883 CONTROL_2 register
 *  @{
 */
#define QMC5883_INT_ENB(N)		(N)		///< Puts N into the INT_ENB field
#define QMC5883_ROL_PNT(N)		((N) << 6)	///< Puts N into the ROL_PNT field
#define QMC5883_SOFT_RST(N)		((N) << 7)	///< Puts N into the SOFT_RST field

#define QMC5883_INT_ENB_MASK	0x01	///< Mask of the INT_ENB field
#define QMC5883_ROL_PNT_MASK	0x40	///< Mask of the ROL_PNT field
#define QMC5883_SOFT_RST_MASK	0x80	///< Mask of the SOFT_RST field
/** @} */


/**
 * @brief Step of pifQmc5883_ReadMagAsync().
 */
typedef enum EnPifQmc5883State
{
	QMC5883_STATE_IDLE,	///< Not reading
	QMC5883_STATE_READ_STATUS,	///< Reading the status
	QMC5883_STATE_READ_DATA	///< Reading the data
} PifQmc5883State;


/**
 * @class StPifQmc5883
 * @brief QMC5883L magnetometer.
 */
typedef struct StPifQmc5883
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;						///< Instance ID
	PifDeviceReg *_p_device;		///< I2C device of the sensor
	PifQmc5883State _state;			///< pifQmc5883_ReadMagAsync() progress

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
	uint8_t __buffer[6];
} PifQmc5883;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks for a QMC5883L at QMC5883_I2C_ADDR: soft-resets it, checks the chip ID
 *        and writes and reads back the SET/RESET period register. Blocks for 20 ms.
 * @param p_i2c I2C port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_Detect(PifI2cPort* p_i2c, void *p_client);

/**
 * @brief Adds the QMC5883L to the I2C port and registers it as the magnetometer of an IMU sensor.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_Init(PifQmc5883* p_owner, PifId id, PifI2cPort* p_i2c, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifQmc5883_Clear(PifQmc5883* p_owner);

/**
 * @brief Writes the CONTROL_1 register and updates the gain of the IMU sensor to its range.
 * @param p_owner Pointer to the owner instance.
 * @param contorl_1 Fields of the register, ORed together.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_SetControl1(PifQmc5883* p_owner, uint8_t contorl_1);

/**
 * @brief Reads the raw values of the 3 axes.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifQmc5883_ReadMag(PifQmc5883* p_owner, int16_t* p_mag);

/**
 * @brief Reads a sample as pifQmc5883_ReadMag() does, waiting for nothing. Each call moves the
 *        read one step on: it starts the STATUS read, then, if DRDY is set, the read of the six
 *        data bytes, and hands the sample over on the call after that one finished. FALSE means
 *        no sample yet, so call again from a later release of the task rather than looping
 *        here. A sample that was not ready, or a failed transfer, starts over at STATUS.
 *        The I2C port has to finish a transfer through pifI2cPort_sigEndTransfer() or its
 *        act_check, and stays taken by this device while _state is not QMC5883_STATE_IDLE, so
 *        pifQmc5883_ReadMag() and other devices on the port fail meanwhile.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Pointer to mag, which receives X, Y and Z only when TRUE is returned.
 * @return TRUE when a new sample was stored in p_mag, otherwise FALSE.
 */
BOOL pifQmc5883_ReadMagAsync(PifQmc5883* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_QMC5883_H
