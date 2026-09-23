#ifndef PIF_ADS1X1X_H
#define PIF_ADS1X1X_H


#include "communication/pif_i2c.h"
#include "core/pif_timer_manager.h"


#define ADS1X1X_I2C_ADDR(N)		(0x48 + (N))

// Conversions no longer than this are waited out with the CPU held instead of through the timer.
// The timer counts in ticks of its manager, so a wait this short either does not fit in one tick
// at all or gets rounded up to one, and a tick can be many times the wait itself: at 3300 SPS the
// conversion takes 303us, which a manager running at 1ms cannot express at all. Holding the CPU
// for it is both shorter and more accurate than what the rounding would cost.
#ifndef PIF_ADS1X1X_SPIN_LIMIT_US
#define PIF_ADS1X1X_SPIN_LIMIT_US	1000UL
#endif


typedef enum EnPifAds1x1xType
{
	ADS1X1X_TYPE_1115		= 0,
	ADS1X1X_TYPE_1114		= 1,
	ADS1X1X_TYPE_1113		= 2,
	ADS1X1X_TYPE_1015		= 3,
	ADS1X1X_TYPE_1014		= 4,
	ADS1X1X_TYPE_1013		= 5
} PifAds1x1xType;

typedef enum EnPifAds1x1xReg
{
    ADS1X1X_REG_CONVERSION,
	ADS1X1X_REG_CONFIG,
	ADS1X1X_REG_LO_THRESH,
	ADS1X1X_REG_HI_THRESH
} PifAds1x1xReg;


// Register : Config

typedef enum EnPifAds1x1xSscs
{
    ADS1X1X_SSCS_NO_EFFECT			= 0 << 15,
	ADS1X1X_SSCS_SINGLE				= 1 << 15
} PifAds1x1xSscs;

typedef enum ENPifAds1x1xMux
{
    ADS1X1X_MUX_DIFF_0_1			= 0 << 12, 	// default
	ADS1X1X_MUX_DIFF_0_3			= 1 << 12,
	ADS1X1X_MUX_DIFF_1_3			= 2 << 12,
	ADS1X1X_MUX_DIFF_2_3			= 3 << 12,
	ADS1X1X_MUX_SINGLE_0			= 4 << 12,
	ADS1X1X_MUX_SINGLE_1			= 5 << 12,
	ADS1X1X_MUX_SINGLE_2			= 6 << 12,
	ADS1X1X_MUX_SINGLE_3			= 7 << 12
} PifAds1x1xMux;

typedef enum EnPifAds1x1xPGA
{
    ADS1X1X_PGA_FSR_6_144V			= 0 << 9,
	ADS1X1X_PGA_FSR_4_096V			= 1 << 9,
	ADS1X1X_PGA_FSR_2_048V			= 2 << 9,	// default
	ADS1X1X_PGA_FSR_1_024V			= 3 << 9,
	ADS1X1X_PGA_FSR_0_512V			= 4 << 9,
	ADS1X1X_PGA_FSR_0_256V			= 5 << 9
} PifAds1x1xPGA;

typedef enum EnPifAds1x1xMode
{
    ADS1X1X_MODE_CONTINUOUS			= 0 << 8,
	ADS1X1X_MODE_SINGLE_SHOT		= 1 << 8
} PifAds1x1xMode;

typedef enum EnPifAds1x1xDR
{
    // for 12bit model
    ADS1X1X_DR_12B_0128_SPS			= 0 << 5,
	ADS1X1X_DR_12B_0250_SPS			= 1 << 5,
	ADS1X1X_DR_12B_0490_SPS			= 2 << 5,
	ADS1X1X_DR_12B_0920_SPS			= 3 << 5,
	ADS1X1X_DR_12B_1600_SPS			= 4 << 5,	// default
	ADS1X1X_DR_12B_2400_SPS			= 5 << 5,
	ADS1X1X_DR_12B_3300_SPS			= 6 << 5,

    // for 16bit model
	ADS1X1X_DR_16B_0008_SPS			= 0 << 5,
	ADS1X1X_DR_16B_0016_SPS			= 1 << 5,
	ADS1X1X_DR_16B_0032_SPS			= 2 << 5,
	ADS1X1X_DR_16B_0064_SPS			= 3 << 5,
	ADS1X1X_DR_16B_0128_SPS			= 4 << 5,	// default
	ADS1X1X_DR_16B_0250_SPS			= 5 << 5,
	ADS1X1X_DR_16B_0475_SPS			= 6 << 5,
	ADS1X1X_DR_16B_0860_SPS			= 7 << 5
} PifAds1x1xDR;

typedef enum EnPifAds1x1xCompMode
{
    ADS1X1X_COMP_MODE_TRADITIONAL	= 0 << 4, 	// default
	ADS1X1X_COMP_MODE_WINDOW		= 1 << 4
} PifAds1x1xCompMode;

typedef enum EnPifAds1x1xCompPol
{
    ADS1X1X_COMP_POL_ACTIVE_L		= 0 << 3, 	// default
	ADS1X1X_COMP_POL_ACTIVE_H		= 1 << 3
} PifAds1x1xCompPol;

typedef enum EnPifAds1x1xCompLat
{
    ADS1X1X_COMP_LAT_DISABLE		= 0 << 2, 	// default
	ADS1X1X_COMP_LAT_ENABLE			= 1 << 2
} PifAds1x1xCompLat;

typedef enum EnPifAds1x1xCompQue
{
    ADS1X1X_COMP_QUE_ONE			= 0,
	ADS1X1X_COMP_QUE_TWO			= 1,
	ADS1X1X_COMP_QUE_FOUR			= 2,
	ADS1X1X_COMP_QUE_DISABLE		= 3
} PifAds1x1xCompQue;

#define ADS1X1X_COMP_QUE_MASK		0x0003	// Comparator queue and disable (ADS1114 and ADS1115 only)
#define ADS1X1X_COMP_LAT_MASK		0x0004	// Latching comparator (ADS1114 and ADS1115 only)
#define ADS1X1X_COMP_POL_MASK		0x0008	// Comparator polarity (ADS1114 and ADS1115 only)
#define ADS1X1X_COMP_MODE_MASK		0x0010	// Comparator mode (ADS1114 and ADS1115 only)
#define ADS1X1X_DR_MASK				0x00E0	// Data rate
#define ADS1X1X_MODE_MASK			0x0100	// Device operating mode
#define ADS1X1X_PGA_MASK			0x0E00	// Programmable gain amplifier configuration
#define ADS1X1X_MUX_MASK			0x7000	// Input multiplexer configuration (ADS1115 only)
#define ADS1X1X_OS_SSCS_MASK		0x8000 	// Operational Status(R) or single-shot conversion start(W) */


struct StPifAds1x1x;
typedef struct StPifAds1x1x PifAds1x1x;

/**
 * @enum EnPifAds1x1xState
 * @brief Whether a single shot conversion is on its way. Watching this is an alternative to the
 *        read event: it leaves ADS1X1X_STATE_CONVERT once the sample has been read into _value.
 */
typedef enum EnPifAds1x1xState
{
	ADS1X1X_STATE_IDLE		= 0,	// Nothing is being converted
	ADS1X1X_STATE_CONVERT	= 1		// A conversion is running and its timer has yet to finish
} PifAds1x1xState;

/**
 * @enum EnPifAds1x1xStart
 * @brief What became of a conversion that pifAds1x1x_StartMux() was asked for. A short enough one
 *        is over by the time the call returns, and this is what says which of the two happened.
 */
typedef enum EnPifAds1x1xStart
{
	ADS1X1X_START_FAILURE	= 0,	// Nothing was started, and pif_error says why
	ADS1X1X_START_TIMED		= 1,	// On its way: the read event brings the sample later
	ADS1X1X_START_DONE		= 2		// Already over: _value holds the sample now
} PifAds1x1xStart;

/**
 * @fn PifEvtAds1x1xRead
 * @brief Reports a conversion that was timed rather than waited out. A conversion that finished
 *        inside pifAds1x1x_StartMux() is reported by its return value instead, so this is never
 *        called from inside that call and starting the next conversion from here is safe.
 * @param p_owner Pointer to the owner instance.
 * @param result TRUE when the conversion register was read; FALSE when that transfer failed, in
 *        which case value carries the previous sample rather than a new one.
 * @param value The raw sample, also left in _value. Multiply by convert_voltage for volts.
 */
typedef void (*PifEvtAds1x1xRead)(PifAds1x1x* p_owner, BOOL result, int16_t value);


/**
 * @class StPifAds1x1x
 * @brief Defines the st pif ads1x1x data structure.
 */
struct StPifAds1x1x
{
	// Public Member Variable
	double convert_voltage;

	// Read-only Member Variable
	PifId _id;
	PifAds1x1xType _type;
	PifI2cDevice* _p_i2c;
    uint16_t _config;
	PifAds1x1xState _state;
	int16_t _value;					// Sample of the most recent single shot conversion

	// Private Member Variable
    uint8_t __resolution;
    uint8_t __channels;
    uint8_t __bit_offset;
    uint32_t __conversion_delay;
	PifTimerManager* __p_timer_manager;
	PifTimer* __p_timer;

	// Private Event Function
	PifEvtAds1x1xRead __evt_read;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifAds1x1x_Init
 * @brief Initializes ads1x1x init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param type Sensor or device type selection.
 * @param port Parameter port used by this operation.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_Init(PifAds1x1x* p_owner, PifId id, PifAds1x1xType type, PifI2cPort* p_port, uint8_t addr, void *p_client);

/**
 * @fn pifAds1x1x_Clear
 * @brief Releases resources used by ads1x1x clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifAds1x1x_Clear(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_Read
 * @brief Reads raw data from ads1x1x read.
 * @param p_owner Pointer to the owner instance.
 * @return Computed integer value.
 */
int16_t pifAds1x1x_Read(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_Voltage
 * @brief Converts the latest sample from ads1x1x voltage into a voltage value.
 * @param p_owner Pointer to the owner instance.
 * @return Computed floating-point value.
 */
double pifAds1x1x_Voltage(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_AttachTimer
 * @brief Gives the instance the timer it needs to convert one channel at a time without holding
 *        the CPU. A single shot conversion takes the inverse of the data rate, up to 125ms at the
 *        slowest of them, and there is nothing to ask the device in the meantime, so the wait is
 *        left to a timer and the result arrives through the callback.
 *        The callback then runs from the timer process of the task manager, which is the same
 *        context a task runs in, so it may read the device, start the next conversion, and do
 *        anything else a task may do.
 *        A conversion of no more than PIF_ADS1X1X_SPIN_LIMIT_US is too short for the timer to
 *        express and is waited out with the CPU held instead. That one is over before
 *        pifAds1x1x_StartMux() returns, so it is reported by the return value and _value rather
 *        than by this callback.
 * @param p_owner Pointer to the owner instance.
 * @param p_timer_manager Timer manager the conversion timer is taken from.
 * @param evt_read Called with the sample once each conversion is over. May be NULL, which leaves
 *        the conversion running with nobody to collect it.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_AttachTimer(PifAds1x1x* p_owner, PifTimerManager* p_timer_manager, PifEvtAds1x1xRead evt_read);

/**
 * @fn pifAds1x1x_DetachTimer
 * @brief Gives the conversion timer back. A conversion that is on its way is abandoned and its
 *        callback never comes.
 * @param p_owner Pointer to the owner instance.
 */
void pifAds1x1x_DetachTimer(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_StartMux
 * @brief Points the multiplexer at one input and starts a single shot conversion there.
 *        A conversion longer than PIF_ADS1X1X_SPIN_LIMIT_US is timed: ADS1X1X_START_TIMED comes
 *        back at once and the sample arrives later through the callback given to
 *        pifAds1x1x_AttachTimer().
 *        A shorter one is waited out with the CPU held, because the timer cannot express it. That
 *        one is finished by the time the call returns: ADS1X1X_START_DONE comes back and _value
 *        already holds the sample, with no callback involved. Nothing therefore runs inside this
 *        call that could start another conversion, so a scan may be driven straight from the
 *        return value.
 *        Only one conversion can be on its way at a time, and only in single shot mode: in
 *        continuous mode the device converts on its own and pifAds1x1x_Read() is what reads it.
 * @param p_owner Pointer to the owner instance.
 * @param mux Input multiplexer channel selection.
 * @return ADS1X1X_START_DONE when the sample is already in _value, ADS1X1X_START_TIMED while it
 *         is still coming, ADS1X1X_START_FAILURE when nothing was started.
 */
PifAds1x1xStart pifAds1x1x_StartMux(PifAds1x1x* p_owner, PifAds1x1xMux mux);

/**
 * @fn pifAds1x1x_SetConfig
 * @brief Sets configuration values required by ads1x1x set config.
 * @param p_owner Pointer to the owner instance.
 * @param config Configuration value to apply.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, uint16_t config);

/**
 * @fn pifAds1x1x_SingleShotConvert
 * @brief Performs the ads1x1x single shot convert operation.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SingleShotConvert(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetMux
 * @brief Sets configuration values required by ads1x1x set mux.
 * @param p_owner Pointer to the owner instance.
 * @param mux Input multiplexer channel selection.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetMux(PifAds1x1x* p_owner, PifAds1x1xMux mux);

/**
 * @fn pifAds1x1x_SetGain
 * @brief Sets configuration values required by ads1x1x set gain.
 * @param p_owner Pointer to the owner instance.
 * @param pga Programmable gain amplifier setting.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetGain(PifAds1x1x* p_owner, PifAds1x1xPGA pga);

/**
 * @fn pifAds1x1x_SetMode
 * @brief Sets configuration values required by ads1x1x set mode.
 * @param p_owner Pointer to the owner instance.
 * @param mode Task operating mode that controls scheduling behavior.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetMode(PifAds1x1x* p_owner, PifAds1x1xMode mode);

/**
 * @fn pifAds1x1x_SetDataRate
 * @brief Sets configuration values required by ads1x1x set data rate.
 * @param p_owner Pointer to the owner instance.
 * @param dr Data rate configuration.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetDataRate(PifAds1x1x* p_owner, PifAds1x1xDR dr);

/**
 * @fn pifAds1x1x_SetCompMode
 * @brief Sets configuration values required by ads1x1x set comp mode.
 * @param p_owner Pointer to the owner instance.
 * @param comp_mode Comparator operating mode.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetCompMode(PifAds1x1x* p_owner, PifAds1x1xCompMode comp_mode);

/**
 * @fn pifAds1x1x_SetCompPol
 * @brief Sets configuration values required by ads1x1x set comp pol.
 * @param p_owner Pointer to the owner instance.
 * @param comp_pol Comparator polarity setting.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetCompPol(PifAds1x1x* p_owner, PifAds1x1xCompPol comp_pol);

/**
 * @fn pifAds1x1x_SetCompLat
 * @brief Sets configuration values required by ads1x1x set comp lat.
 * @param p_owner Pointer to the owner instance.
 * @param comp_lat Comparator latching behavior setting.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetCompLat(PifAds1x1x* p_owner, PifAds1x1xCompLat comp_lat);

/**
 * @fn pifAds1x1x_SetCompQue
 * @brief Sets configuration values required by ads1x1x set comp que.
 * @param p_owner Pointer to the owner instance.
 * @param comp_que Comparator queue setting.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetCompQue(PifAds1x1x* p_owner, PifAds1x1xCompQue comp_que);

/**
 * @fn pifAds1x1x_SetLoThresh
 * @brief Sets configuration values required by ads1x1x set lo thresh.
 * @param p_owner Pointer to the owner instance.
 * @param threshold Threshold value to apply.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetLoThresh(PifAds1x1x* p_owner, int16_t threshold);

/**
 * @fn pifAds1x1x_GetLoThresh
 * @brief Retrieves the current value from ads1x1x get lo thresh.
 * @param p_owner Pointer to the owner instance.
 * @return Computed integer value.
 */
int16_t pifAds1x1x_GetLoThresh(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetLoThreshVoltage
 * @brief Sets configuration values required by ads1x1x set lo thresh voltage.
 * @param p_owner Pointer to the owner instance.
 * @param threshold Threshold value to apply.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetLoThreshVoltage(PifAds1x1x* p_owner, double threshold);

/**
 * @fn pifAds1x1x_GetLoThreshVoltage
 * @brief Retrieves the current value from ads1x1x get lo thresh voltage.
 * @param p_owner Pointer to the owner instance.
 * @return Computed floating-point value.
 */
double pifAds1x1x_GetLoThreshVoltage(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetHiThresh
 * @brief Sets configuration values required by ads1x1x set hi thresh.
 * @param p_owner Pointer to the owner instance.
 * @param threshold Threshold value to apply.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetHiThresh(PifAds1x1x* p_owner, int16_t threshold);

/**
 * @fn pifAds1x1x_GetHiThresh
 * @brief Retrieves the current value from ads1x1x get hi thresh.
 * @param p_owner Pointer to the owner instance.
 * @return Computed integer value.
 */
int16_t pifAds1x1x_GetHiThresh(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetHiThreshVoltage
 * @brief Sets configuration values required by ads1x1x set hi thresh voltage.
 * @param p_owner Pointer to the owner instance.
 * @param threshold Threshold value to apply.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifAds1x1x_SetHiThreshVoltage(PifAds1x1x* p_owner, double threshold);

/**
 * @fn pifAds1x1x_GetHiThreshVoltage
 * @brief Retrieves the current value from ads1x1x get hi thresh voltage.
 * @param p_owner Pointer to the owner instance.
 * @return Computed floating-point value.
 */
double pifAds1x1x_GetHiThreshVoltage(PifAds1x1x* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ADS1X1X_H
