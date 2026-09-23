#ifndef PIF_MS5611_H
#define PIF_MS5611_H


#include "communication/pif_i2c.h"
#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"
#include "sensor/pif_sensor_event.h"


#define	MS5611_I2C_ADDR(N)		(0x76 + (N))


typedef enum EnPifMs5611Reg
{
	MS5611_REG_ADC_READ			= 0x00,
	MS5611_REG_RESET			= 0x1E,
	MS5611_REG_CONV_D1			= 0x40,		// Pressure
	MS5611_REG_CONV_D2			= 0x50,		// Temperature
	MS5611_REG_READ_PROM		= 0xA0
} PifMs5611Reg;

typedef enum EnPifMs5611Osr
{
	MS5611_OSR_256				= 0x00,
	MS5611_OSR_512				= 0x02,
	MS5611_OSR_1024				= 0x04,		// Default
	MS5611_OSR_2048				= 0x06,
	MS5611_OSR_4096				= 0x08
} PifMs5611Osr;

/**
 * @enum EnPifMs5611State
 * @brief How far a barometric reading has got. A reading is two conversions and a calculation, and
 *        this walks the same steps whichever of the two drivers is carrying it: the task added by
 *        pifMs5611_AttachTaskForReading() or the timer added by pifMs5611_AttachTimer(). They
 *        share this field, so attach one or the other rather than both.
 *        MS5611_STATE_IDLE means no reading is on its way, which is where the timer leaves it
 *        between readings.
 */
typedef enum EnPifMs5611State
{
	MS5611_STATE_IDLE,
	MS5611_STATE_TEMPERATURE_START,
	MS5611_STATE_TEMPERATURE_WAIT,
	MS5611_STATE_PRESSURE_START,
	MS5611_STATE_PRESSURE_WAIT,
	MS5611_STATE_CALCURATE
} PifMs5611State;

struct StPifMs5611;
typedef struct StPifMs5611 PifMs5611;

/**
 * @fn PifEvtMs5611Read
 * @brief Reports a barometric reading that pifMs5611_StartBarometric() asked for.
 * @param p_owner Pointer to the owner instance.
 * @param result TRUE when the reading finished; FALSE when one of its transfers failed, in which
 *        case there are no values and the two below mean nothing.
 * @param pressure unit : hPa
 * @param temperature unit : degrees C
 */
typedef void (*PifEvtMs5611Read)(PifMs5611* p_owner, BOOL result, float pressure, float temperature);


/**
 * @class StPifMs5611
 * @brief Defines the st pif ms5611 data structure.
 */
struct StPifMs5611
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;
	uint16_t _prom[8];
	uint16_t _over_sampling_rate;
	uint8_t _conversion_time;
	PifTask* _p_task;
	PifMs5611State _state;

	// Private Member Variable
	uint16_t __read_period;
	uint32_t __D1, __D2;
	uint32_t __start_time;
	PifTimerManager* __p_timer_manager;
	PifTimer* __p_timer;

	// Private Event Function
	PifEvtBaroRead __evt_read;
	PifEvtMs5611Read __evt_timer_read;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMs5611_Init
 * @brief Initializes ms5611 init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMs5611_Init(PifMs5611* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifMs5611_Clear
 * @brief Releases resources used by ms5611 clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifMs5611_Clear(PifMs5611* p_owner);

/**
 * @fn pifMs5611_SetOverSamplingRate
 * @brief Sets configuration values required by ms5611 set over sampling rate.
 * @param p_owner Pointer to the owner instance.
 * @param osr Parameter osr used by this operation.
 */
void pifMs5611_SetOverSamplingRate(PifMs5611* p_owner, uint16_t osr);

/**
 * @fn pifMs5611_StartBarometric
 * @brief Starts one barometric reading and returns at once. Both conversions and the calculation
 *        that follows them are carried by the timer, and the values arrive through the callback
 *        given to pifMs5611_AttachTimer().
 *        Only one reading can be on its way at a time: _state says whether one is.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE once the reading is started, otherwise FALSE.
 */
BOOL pifMs5611_StartBarometric(PifMs5611* p_owner);

/**
 * @fn pifMs5611_AttachTimer
 * @brief Gives the instance the timer it needs to take a reading on demand without holding the
 *        CPU. The device has no register to ask whether a conversion is over, so the only thing
 *        to do is leave it for the conversion time, which is 2 to 11ms depending on the
 *        oversampling rate and happens twice per reading. That wait is left to a timer and the
 *        result arrives through the callback.
 *        The callback runs from the timer process of the task manager, which is the same context
 *        a task runs in, so it may read the device, ask for the next reading, and do anything
 *        else a task may do.
 * @param p_owner Pointer to the owner instance.
 * @param p_timer_manager Timer manager the conversion timer is taken from.
 * @param evt_read Called once per reading, and the only place the values are reported, so a
 *        reading started without one is thrown away. _state says whether a reading is on its way.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMs5611_AttachTimer(PifMs5611* p_owner, PifTimerManager* p_timer_manager, PifEvtMs5611Read evt_read);

/**
 * @fn pifMs5611_DetachTimer
 * @brief Gives the conversion timer back. A reading on its way is abandoned and its callback
 *        never comes.
 * @param p_owner Pointer to the owner instance.
 */
void pifMs5611_DetachTimer(PifMs5611* p_owner);

/**
 * @fn pifMs5611_AttachTaskForReading
 * @brief Creates and attaches a task for ms5611 attach task for reading processing.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param read_period Parameter read_period used by this operation.
 * @param evt_read Parameter evt_read used by this operation.
 * @param start Set to TRUE to start the task immediately.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMs5611_AttachTaskForReading(PifMs5611* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MS5611_H
