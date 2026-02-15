#ifndef PIF_DPS310_H
#define PIF_DPS310_H


#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"
#include "core/pif_task_manager.h"
#include "sensor/pif_sensor_event.h"


#define DPS310_I2C_ADDR(N)		(0x76 + (N))	// 0 ~ 1, default = 1

#define DPS310_PRODUCT_ID_CONST	0x10


typedef enum EnPifDps310Reg
{
	DPS310_REG_PRS_B2			= 0x00,
	DPS310_REG_PRS_B1			= 0x01,
	DPS310_REG_PRS_B0			= 0x02,
	DPS310_REG_TMP_B2			= 0x03,
	DPS310_REG_TMP_B1			= 0x04,
	DPS310_REG_TMP_B0			= 0x05,
	DPS310_REG_PRS_CFG			= 0x06,
	DPS310_REG_TMP_CFG			= 0x07,
	DPS310_REG_MEAS_CFG			= 0x08,
	DPS310_REG_CFG_REG			= 0x09,
	DPS310_REG_INT_STS			= 0x0A,
	DPS310_REG_FIFO_STS			= 0x0B,
	DPS310_REG_RESET			= 0x0C,
	DPS310_REG_PRODUCT_ID		= 0x0D,
	DPS310_REG_COEF				= 0x10,
	DPS310_REG_COEF_SPCE		= 0x28
} PifDps310Reg;


// Register : PRS_CFG

typedef enum EnPifDps310PmPrc
{
	DPS310_PM_PRC_SINGLE		= 0,
	DPS310_PM_PRC_2				= 1,
	DPS310_PM_PRC_4				= 2,
	DPS310_PM_PRC_8				= 3,
	DPS310_PM_PRC_16			= 4,
	DPS310_PM_PRC_32			= 5,
	DPS310_PM_PRC_64			= 6,
	DPS310_PM_PRC_128			= 7
} PifDps310PmPrc;

typedef enum EnPifDps310PmRate
{
	DPS310_PM_RATE_1HZ			= 0 << 4,
	DPS310_PM_RATE_2HZ			= 1 << 4,
	DPS310_PM_RATE_4HZ			= 2 << 4,
	DPS310_PM_RATE_8HZ			= 3 << 4,
	DPS310_PM_RATE_16HZ			= 4 << 4,
	DPS310_PM_RATE_32HZ			= 5 << 4,
	DPS310_PM_RATE_64HZ			= 6 << 4,
	DPS310_PM_RATE_128HZ		= 7 << 4
} PifDps310PmRate;

#define DPS310_PM_PRC_MASK		0x0F
#define DPS310_PM_RATE_MASK		0x70


// Register : TMP_CFG

typedef enum EnPifDps310TmpPrc
{
	DPS310_TMP_PRC_SINGLE		= 0,
	DPS310_TMP_PRC_2			= 1,
	DPS310_TMP_PRC_4			= 2,
	DPS310_TMP_PRC_8			= 3,
	DPS310_TMP_PRC_16			= 4,
	DPS310_TMP_PRC_32			= 5,
	DPS310_TMP_PRC_64			= 6,
	DPS310_TMP_PRC_128			= 7
} PifDps310TmpPrc;

typedef enum EnPifDps310TmpRate
{
	DPS310_TMP_RATE_1HZ			= 0 << 4,
	DPS310_TMP_RATE_2HZ			= 1 << 4,
	DPS310_TMP_RATE_4HZ			= 2 << 4,
	DPS310_TMP_RATE_8HZ			= 3 << 4,
	DPS310_TMP_RATE_16HZ		= 4 << 4,
	DPS310_TMP_RATE_32HZ		= 5 << 4,
	DPS310_TMP_RATE_64HZ		= 6 << 4,
	DPS310_TMP_RATE_128HZ		= 7 << 4
} PifDps310TmpRate;

#define DPS310_TMP_EXT(N)		((N) << 7)

#define DPS310_TMP_PRC_MASK		0x0F
#define DPS310_TMP_RATE_MASK	0x70
#define DPS310_TMP_EXT_MASK		0x80


// Register : MEAS_CFG

typedef enum EnPifDps310MeasCtrl
{
	DPS310_MEAS_CTRL_IDLE			= 0,
	DPS310_MEAS_CTRL_PRS			= 1,
	DPS310_MEAS_CTRL_TMP			= 2,
	DPS310_MEAS_CTRL_CONT_PRS		= 5,
	DPS310_MEAS_CTRL_CONT_TMP		= 6,
	DPS310_MEAS_CTRL_CONT_PRS_TMP	= 7
} PifDps310MeasCtrl;

#define DPS310_PRS_RDY(N)			((N) << 4)
#define DPS310_TMP_RDY(N)			((N) << 5)
#define DPS310_SENSOR_RDY(N)		((N) << 6)
#define DPS310_COEF_RDY(N)			((N) << 7)

#define DPS310_MEAS_CTRL_MASK		0x07
#define DPS310_PRS_RDY_MASK			0x10
#define DPS310_TMP_RDY_MASK			0x20
#define DPS310_SENSOR_RDY_MASK		0x40
#define DPS310_COEF_RDY_MASK		0x80


// Register : CFG_REG

#define DPS310_SPI_MODE(N)		(N)
#define DPS310_FIFO_EN(N)		((N) << 1)
#define DPS310_P_SHIFT(N)		((N) << 2)
#define DPS310_T_SHIFT(N)		((N) << 3)
#define DPS310_INT_PRS(N)		((N) << 4)
#define DPS310_INT_TMP(N)		((N) << 5)
#define DPS310_INT_FIFO(N)		((N) << 6)
#define DPS310_INT_HL(N)		((N) << 7)

#define DPS310_SPI_MODE_MASK	0x01
#define DPS310_FIFO_EN_MASK		0x02
#define DPS310_P_SHIFT_MASK		0x04
#define DPS310_T_SHIFT_MASK		0x08
#define DPS310_INT_PRS_MASK		0x10
#define DPS310_INT_TMP_MASK		0x20
#define DPS310_INT_FIFO_MASK	0x40
#define DPS310_INT_HL_MASK		0x80


// Register : INT_STS

#define DPS310_INT_STS_PRS(N)		(N)
#define DPS310_INT_STS_TMP(N)		((N) << 1)
#define DPS310_INT_FIFO_FULL(N)		((N) << 2)

#define DPS310_INT_STS_PRS_MASK		0x01
#define DPS310_INT_STS_TMP_MASK		0x02
#define DPS310_INT_FIFO_FULL_MASK	0x04


// Register : FIFO_STS

#define DPS310_FIFO_EMPTY(N)	(N)
#define DPS310_FIFO_FULL(N)		((N) << 1)

#define DPS310_FIFO_EMPTY_MASK	0x01
#define DPS310_FIFO_FULL_MASK	0x02


// Register : RESET

typedef enum EnPifDps310SoftRst
{
	DPS310_RESET_SOFT_RST		= 9
} PifDps310SoftRst;

#define DPS310_FIFO_FLUSH(N)	((N) << 7)

#define DPS310_SOFT_RST_MASK	0x0F
#define DPS310_FIFO_FLUSH_MASK	0x80


// Register : PRODUCT_ID

#define DPS310_PROD_ID(N)		(N)
#define DPS310_REV_ID(N)		((N) << 4)

#define DPS310_PROD_ID_MASK		0x0F
#define DPS310_REV_ID_MASK		0xF0


// Register : COEF_SPCE

#define DPS310_TMP_COEF_SPCE(N)		((N) << 7)

#define DPS310_TMP_COEF_SPCE_MASK	0x80


typedef enum EnPifDps310State
{
	DPS310_STATE_IDLE,
	DPS310_STATE_READY,
	DPS310_STATE_READ,
	DPS310_STATE_CALCURATE
} PifDps310State;


/**
 * @class StPifDps310
 * @brief Defines the st pif dps310 data structure.
 */
typedef struct StPifDps310
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	union {
		PifI2cDevice* _p_i2c;
		PifSpiDevice* _p_spi;
	};
	uint8_t _osrs_p;
	uint8_t _osrs_t;
	PifTask* _p_task;

	// Private Member Variable
	uint16_t __read_period;
	PifDps310State __state;
	uint32_t __start_time;
	int32_t __raw_pressure;
	int32_t __raw_temperature;
	float __last_temp;
	//compensation coefficients
	int32_t __c0_half;
	int32_t __c1;
	int32_t __c00;
	int32_t __c10;
	int32_t __c01;
	int32_t __c11;
	int32_t __c20;
	int32_t __c21;
	int32_t __c30;

	// Read-only Function
	PifDeviceReg8Func _fn;

	// Private Event Function
	PifEvtBaroRead __evt_read;
} PifDps310;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDps310_Config
 * @brief Performs the dps310 config operation.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_Config(PifDps310* p_owner, PifId id);

/**
 * @fn pifDps310_SetPressureCfg
 * @brief Sets configuration values required by dps310 set pressure cfg.
 * @param p_owner Pointer to the owner instance.
 * @param osrs Parameter osrs used by this operation.
 * @param rate Parameter rate used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_SetPressureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate);

/**
 * @fn pifDps310_SetTemperatureCfg
 * @brief Sets configuration values required by dps310 set temperature cfg.
 * @param p_owner Pointer to the owner instance.
 * @param osrs Parameter osrs used by this operation.
 * @param rate Parameter rate used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_SetTemperatureCfg(PifDps310* p_owner, uint8_t osrs, uint8_t rate);

/**
 * @fn pifDps310_SensorOperatingMode
 * @brief Performs the dps310 sensor operating mode operation.
 * @param p_owner Pointer to the owner instance.
 * @param meas_ctrl Parameter meas_ctrl used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_SensorOperatingMode(PifDps310* p_owner, uint8_t meas_ctrl);

/**
 * @fn pifDps310_ReadRawData
 * @brief Reads raw data from dps310 read raw data.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Pointer to pressure.
 * @param p_temperature Pointer to temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_ReadRawData(PifDps310* p_owner, int32_t* p_pressure, int32_t* p_temperature);

/**
 * @fn pifDps310_ReadBarometric
 * @brief Reads raw data from dps310 read barometric.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Pointer to pressure.
 * @param p_temperature Pointer to temperature.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_ReadBarometric(PifDps310* p_owner, float* p_pressure, float* p_temperature);

/**
 * @fn pifDps310_AttachTaskForReading
 * @brief Creates and attaches a task for dps310 attach task for reading processing.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param read_period Parameter read_period used by this operation.
 * @param evt_read Parameter evt_read used by this operation.
 * @param start Set to TRUE to start the task immediately.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310_AttachTaskForReading(PifDps310* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_H
