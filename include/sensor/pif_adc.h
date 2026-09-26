#ifndef PIF_ADC_H
#define PIF_ADC_H


#include "core/pif_task_manager.h"


/*
 * A set of ADC channels read together, turned into millivolts and then into whatever each channel
 * measures: a battery voltage through a divider, a current through a shunt amplifier, an RSSI, a
 * temperature.
 *
 * What is hardware is left to the client: act_read gives the latest conversion of a channel, which
 * is a load from the DMA buffer on an MCU that scans its channels into one, and act_start, if there
 * is one, starts the next round of conversions on an MCU that converts on request. Everything after
 * the raw value is here:
 *
 *     raw --filter--> filtered raw --reference--> millivolts --scale or calibration--> value
 *
 * The reference is the supply of the ADC, taken as the nominal one given to pifAdc_Init(), or
 * measured through the internal reference of the MCU when a channel is set up for it with
 * pifAdc_SetVrefint(), which is what keeps readings right while the supply sags.
 */


#ifndef PIF_ADC_MAX_CHANNELS
#define PIF_ADC_MAX_CHANNELS		8
#endif


typedef enum EnPifAdcChannelType
{
	ACH_VOLTAGE			= 0,	// value = millivolts * mul / div + offset
	ACH_VREFINT			= 1,	// Measures the internal reference; value = the supply in millivolts
	ACH_TEMPERATURE		= 2		// Two-point calibrated sensor; value = tenths of a degree Celsius
} PifAdcChannelType;


typedef struct StPifAdcChannel
{
	// Read-only Member Variable
	PifAdcChannelType _type;
	uint16_t _raw;				// Filtered raw value
	uint16_t _millivolt;
	int32_t _value;

	// Private Member Variable
	uint32_t __filtered;		// Raw value with _filter_shift bits of fraction
	int32_t __mul;
	int32_t __div;
	int32_t __offset;
	uint16_t __cal1_raw;		// ACH_VREFINT: reading at __cal_mv; ACH_TEMPERATURE: reading at __cal1_t
	uint16_t __cal2_raw;		// ACH_TEMPERATURE: reading at __cal2_t
	int16_t __cal1_t;			// Tenths of a degree
	int16_t __cal2_t;
	uint16_t __cal_mv;			// Supply the calibration readings were taken at
	BOOL __primed;				// The filter holds a first reading
} PifAdcChannel;


struct StPifAdc;
typedef struct StPifAdc PifAdc;

/**
 * @fn PifActAdcRead
 * @brief Gives the latest conversion of a channel.
 * @param p_owner Pointer to the ADC instance.
 * @param index Channel, 0 to _channel_count - 1, as the client numbered them.
 * @return Raw conversion, right-aligned.
 */
typedef uint16_t (*PifActAdcRead)(PifAdc* p_owner, uint8_t index);

/**
 * @fn PifActAdcStart
 * @brief Starts the next round of conversions, after pifAdc_Sample() has read the last one.
 * @param p_owner Pointer to the ADC instance.
 */
typedef void (*PifActAdcStart)(PifAdc* p_owner);

/**
 * @fn PifEvtAdcSample
 * @brief Reports that every channel has a new value, from inside pifAdc_Sample().
 * @param p_owner Pointer to the ADC instance.
 */
typedef void (*PifEvtAdcSample)(PifAdc* p_owner);


/**
 * @class StPifAdc
 * @brief Multi-channel ADC with filtering, supply compensation and per-channel scaling.
 */
struct StPifAdc
{
	// Public Event Function
	PifEvtAdcSample evt_sample;

	// Read-only Member Variable
	PifId _id;
	PifTask* _p_task;
	uint8_t _channel_count;
	uint8_t _resolution;		// Bits of a conversion
	uint8_t _filter_shift;
	uint16_t _vref_mv;			// Supply of the ADC as last measured, or the nominal one
	PifAdcChannel _channel[PIF_ADC_MAX_CHANNELS];

	// Private Member Variable
	uint8_t __vrefint_index;	// Channel set up with pifAdc_SetVrefint(), or 0xFF

	// Private Action Function
	PifActAdcRead __act_read;
	PifActAdcStart __act_start;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifAdc_Init
 * @brief Initializes an ADC instance. Every channel starts as ACH_VOLTAGE with a value in
 *        millivolts at the pin.
 * @param p_owner Pointer to the instance.
 * @param id Identifier, or PIF_ID_AUTO.
 * @param channel_count Number of channels, 1 to PIF_ADC_MAX_CHANNELS.
 * @param resolution Bits of a conversion, 8 to 16.
 * @param vref_mv Nominal supply of the ADC, 3300 for instance.
 * @param act_read Gives the latest conversion of a channel.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAdc_Init(PifAdc* p_owner, PifId id, uint8_t channel_count, uint8_t resolution, uint16_t vref_mv, PifActAdcRead act_read);

/**
 * @fn pifAdc_Clear
 * @brief Removes the task of the instance, if it has one.
 * @param p_owner Pointer to the instance.
 */
void pifAdc_Clear(PifAdc* p_owner);

/**
 * @fn pifAdc_AttachActStart
 * @brief Gives the instance a way to start conversions, for an ADC that converts on request. It
 *        is called at the end of every pifAdc_Sample(), and once from here to have the first
 *        conversions ready by the first sample.
 * @param p_owner Pointer to the instance.
 * @param act_start Starts the next round of conversions.
 */
void pifAdc_AttachActStart(PifAdc* p_owner, PifActAdcStart act_start);

/**
 * @fn pifAdc_AttachTask
 * @brief Samples every channel from a task of its own.
 * @param p_owner Pointer to the instance.
 * @param period1us Sampling period.
 * @param start TRUE to start sampling at once.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAdc_AttachTask(PifAdc* p_owner, uint32_t period1us, BOOL start);

/**
 * @fn pifAdc_SetFilter
 * @brief Sets the low-pass filter every raw value goes through, filtered += (raw - filtered) / 2^shift.
 *        The time constant is about 2^shift samples; 0 turns it off.
 * @param p_owner Pointer to the instance.
 * @param shift 0 to 8.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAdc_SetFilter(PifAdc* p_owner, uint8_t shift);

/**
 * @fn pifAdc_SetScale
 * @brief Makes a channel ACH_VOLTAGE with value = millivolts * mul / div + offset. A battery on a
 *        10k:1k divider is mul 11, div 1. A current sensor of S mV per ampere with O mV at 0 A,
 *        read in milliamperes, is mul 1000, div S, offset -O * 1000 / S.
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @param mul Multiplier.
 * @param div Divisor, not 0.
 * @param offset Added after the division.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAdc_SetScale(PifAdc* p_owner, uint8_t index, int32_t mul, int32_t div, int32_t offset);

/**
 * @fn pifAdc_SetVrefint
 * @brief Makes a channel the one that reads the internal reference of the MCU, and from then on
 *        the supply is measured through it: _vref_mv = cal_mv * cal_raw / reading. On STM32,
 *        cal_raw is the factory VREFINT_CAL value and cal_mv the supply it was taken at, 3300.
 *        With only the nominal reference voltage Vr known, cal_raw is Vr * full scale / cal_mv.
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @param cal_raw Reading of the reference at a supply of cal_mv.
 * @param cal_mv Supply of that reading.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAdc_SetVrefint(PifAdc* p_owner, uint8_t index, uint16_t cal_raw, uint16_t cal_mv);

/**
 * @fn pifAdc_SetReference
 * @brief Sets the supply the channels are converted against, for an instance that has no channel
 *        of its own on the internal reference: one ADC that measures the supply can hand it on to
 *        another that shares it. An instance with such a channel measures it again on every
 *        sample, which overrides this.
 * @param p_owner Pointer to the instance.
 * @param vref_mv Supply of the ADC, in millivolts, not 0.
 */
void pifAdc_SetReference(PifAdc* p_owner, uint16_t vref_mv);

/**
 * @fn pifAdc_SetTemperature
 * @brief Makes a channel a temperature sensor calibrated at two points, as STM32 parts are at the
 *        factory (TS_CAL1 at 30 degrees, TS_CAL2 at 110 or 130). Readings are brought to the
 *        supply the calibration was taken at before they are compared with it.
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @param cal1_raw Reading at cal1_t.
 * @param cal1_t First calibration temperature, in tenths of a degree Celsius.
 * @param cal2_raw Reading at cal2_t, not equal to cal1_raw.
 * @param cal2_t Second calibration temperature, in tenths of a degree Celsius.
 * @param cal_mv Supply both readings were taken at.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAdc_SetTemperature(PifAdc* p_owner, uint8_t index, uint16_t cal1_raw, int16_t cal1_t, uint16_t cal2_raw, int16_t cal2_t, uint16_t cal_mv);

/**
 * @fn pifAdc_Sample
 * @brief Reads every channel, filters it, measures the supply if a channel reads the internal
 *        reference, converts every channel, starts the next conversions and reports through
 *        evt_sample. The task of pifAdc_AttachTask() calls it; without one the client does.
 * @param p_owner Pointer to the instance.
 */
void pifAdc_Sample(PifAdc* p_owner);

/**
 * @fn pifAdc_SampleChannel
 * @brief Reads, filters and converts one channel alone, for channels that are read on demand and
 *        at rates of their own rather than all together. The supply it is converted against is the
 *        last one measured or set; the channel on the internal reference, sampled this way,
 *        measures it again. Neither act_start nor evt_sample is called.
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @return Value of the channel, as pifAdc_GetValue() gives it.
 */
int32_t pifAdc_SampleChannel(PifAdc* p_owner, uint8_t index);

/**
 * @fn pifAdc_GetRaw
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @return Filtered raw value of the channel.
 */
uint16_t pifAdc_GetRaw(PifAdc* p_owner, uint8_t index);

/**
 * @fn pifAdc_GetMilliVolt
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @return Voltage at the pin of the channel, in millivolts.
 */
uint16_t pifAdc_GetMilliVolt(PifAdc* p_owner, uint8_t index);

/**
 * @fn pifAdc_GetValue
 * @param p_owner Pointer to the instance.
 * @param index Channel.
 * @return Value of the channel, in the unit its type gives it.
 */
int32_t pifAdc_GetValue(PifAdc* p_owner, uint8_t index);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ADC_H
