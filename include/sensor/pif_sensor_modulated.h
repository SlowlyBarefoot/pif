// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_sensor_modulated.h
 * @brief Levels of optical sensors measured with their emitter LED on and off.
 *
 * One PifSensorModulated switches an emitter LED that its channels share, through act_led, and steps a cycle
 * on a task: LED on, settle, sample, LED off, settle, sample. The samples come in one of two ways, and only those
 * that fall in a sample window count:
 *
 * - From the ADC interrupt, with pifSensorModulated_sigSample().
 * - Read by the task itself, with the act_acquire of pifSensorModulated_AttachActAcquire(), at a fixed period
 *   and only while a sample window is open.
 *
 * At the end of every cycle each channel has its average level with the LED on, its average level with the LED
 * off, and their difference, which is the light of the LED alone without the ambient light.
 *
 * A channel may hand one of these levels to a PifSensorThreshold, which turns it into an ON/OFF state and
 * reports the changes. The LED is driven only through act_led, so an LED kept in a PifLed is switched there
 * with pifLed_PartChange(). Samples taken from a PifAdc go in through its evt_sample with pifAdc_GetRaw(), and
 * its filter has to be off: it would mix the levels with the LED on and off.
 */
#ifndef PIF_SENSOR_MODULATED_H
#define PIF_SENSOR_MODULATED_H


#include "core/pif_task_manager.h"
#include "sensor/pif_sensor_threshold.h"


/**
 * @brief Level of a channel.
 */
typedef enum EnPifSensorModulatedLevel
{
	SML_ON			= 0,	///< Average with the LED on
	SML_OFF			= 1,	///< Average with the LED off, the ambient light
	SML_DIFF		= 2		///< SML_ON minus SML_OFF, the light of the LED alone
} PifSensorModulatedLevel;

/**
 * @brief Step of the measuring cycle.
 */
typedef enum EnPifSensorModulatedPhase
{
	SMP_IDLE		= 0,	///< Stopped
	SMP_ON_SETTLE	= 1,	///< LED on, waiting for the light to settle
	SMP_ON_SAMPLE	= 2,	///< LED on, samples count
	SMP_OFF_SETTLE	= 3,	///< LED off, waiting for the light to settle
	SMP_OFF_SAMPLE	= 4		///< LED off, samples count
} PifSensorModulatedPhase;

struct StPifSensorModulated;
typedef struct StPifSensorModulated PifSensorModulated;

/**
 * @brief Switches the emitter LED of the channels ON or OFF.
 */
typedef void (*PifActSensorModulatedLed)(PifSensorModulated* p_owner, SWITCH state);

/**
 * @brief Reads a sample of a channel, for pifSensorModulated_AttachActAcquire(). It is called from the task and
 *        has to return at once, so a conversion that takes long, such as one of an ADC on a bus, belongs to
 *        pifSensorModulated_sigSample() instead.
 */
typedef int32_t (*PifActSensorModulatedAcquire)(PifSensorModulated* p_owner, uint8_t channel);

/**
 * @brief Reports the end of a cycle, after the levels of every channel are renewed and handed to their sensors.
 */
typedef void (*PifEvtSensorModulatedCycle)(PifSensorModulated* p_owner);

/**
 * @class StPifSensorModulatedChannel
 * @brief Levels and attached sensor of one channel.
 */
typedef struct StPifSensorModulatedChannel
{
	// Read-only Member Variable
	int32_t _on_level;				///< Average of the last window with the LED on
	int32_t _off_level;				///< Average of the last window with the LED off

	// Private Member Variable
	volatile int32_t __sum;			///< Samples of the current window, added by pifSensorModulated_sigSample()
	volatile uint16_t __count;
	BOOL __on_valid;				///< The last on window had samples
	BOOL __off_valid;				///< The last off window had samples
	PifSensorThreshold* __p_sensor;
	PifSensorModulatedLevel __level;
} PifSensorModulatedChannel;

/**
 * @class StPifSensorModulated
 * @brief Emitter LED shared by up to 255 channels, measured on one TM_EXTERNAL task.
 *
 * The task runs in the main context and the samples may come from an interrupt. The interrupt only adds to the
 * window of a channel while a sample phase is open, and the task opens and closes the windows, so they need no
 * lock. Samples of act_acquire are added by the task inside pif_IrqDisable(), so both ways may be used together
 * once pif_act_irq_disable and pif_act_irq_restore are set.
 */
struct StPifSensorModulated
{
	// Public Event Function
	PifEvtSensorModulatedCycle evt_cycle;	///< Called at the end of every cycle. May be NULL.

	// Read-only Member Variable
	PifId _id;								///< Instance ID
	uint8_t _channel_count;					///< Number of channels
	volatile PifSensorModulatedPhase _phase;	///< Current step of the cycle
	uint32_t _cycle_count;					///< Cycles completed since pifSensorModulated_Start()
	PifTask* _p_task;						///< Task that steps the cycle

	// Private Member Variable
	BOOL __running;							///< Between pifSensorModulated_Start() and pifSensorModulated_Stop()
	uint32_t __settle_us;
	uint32_t __sample_us;
	uint32_t __acquire_us;					///< Period of act_acquire
	BOOL __window_acquire;					///< The open sample window is read with act_acquire
	uint32_t __window_end;					///< End of the open sample window, on the 1us clock
	PifSensorModulatedChannel* __p_channel;

	// Private Action Function
	PifActSensorModulatedLed __act_led;
	PifActSensorModulatedAcquire __act_acquire;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a modulated sensor and adds its task, so pifTaskManager_Init() must have room for one more task.
 *        It stays stopped until pifSensorModulated_SetTiming() and pifSensorModulated_Start().
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param channel_count Number of channels, 1 to 255.
 * @param act_led Switches the emitter LED.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifSensorModulated_Init(PifSensorModulated* p_owner, PifId id, uint8_t channel_count, PifActSensorModulatedLed act_led);

/**
 * @brief Stops the cycle, removes the task and frees the channels.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorModulated_Clear(PifSensorModulated* p_owner);

/**
 * @brief Sets the timing of a cycle. Each half takes settle_us and then sample_us, so a cycle takes
 *        2 * (settle_us + sample_us). A change while running takes effect from the next step.
 * @param p_owner Pointer to the owner instance.
 * @param settle_us Time after the LED is switched before the samples count, 0 for none.
 * @param sample_us Time during which the samples count, 1 or more.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when sample_us is 0.
 */
BOOL pifSensorModulated_SetTiming(PifSensorModulated* p_owner, uint32_t settle_us, uint32_t sample_us);

/**
 * @brief Lets the task read the samples itself: every channel at the opening of a sample window and then every
 *        period_us until it closes. A change while running takes effect from the next window.
 * @param p_owner Pointer to the owner instance.
 * @param act_acquire Reads a sample of a channel, or NULL to stop reading.
 * @param period_us Time between two reads, 1 or more. A period of sample_us or more reads once per window.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when act_acquire is given with a period of 0.
 */
BOOL pifSensorModulated_AttachActAcquire(PifSensorModulated* p_owner, PifActSensorModulatedAcquire act_acquire,
		uint32_t period_us);

/**
 * @brief Hands one level of a channel to a threshold sensor at the end of every cycle, followed by
 *        pifSensorThreshold_ProcessAcquire(). The sensor should have no act_acquire, and no filter when the
 *        windows already average enough. A level whose window had no samples is not handed over.
 * @param p_owner Pointer to the owner instance.
 * @param channel Channel, less than channel_count.
 * @param p_sensor Threshold sensor, or NULL to detach the one attached.
 * @param level Level handed to it.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifSensorModulated_AttachSensor(PifSensorModulated* p_owner, uint8_t channel, PifSensorThreshold* p_sensor,
		PifSensorModulatedLevel level);

/**
 * @brief Starts the cycle from the LED on step. Calling it while running starts the cycle over.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE with E_INVALID_STATE before pifSensorModulated_SetTiming().
 */
BOOL pifSensorModulated_Start(PifSensorModulated* p_owner);

/**
 * @brief Stops the cycle and switches the LED off. The levels of the last cycle stay.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorModulated_Stop(PifSensorModulated* p_owner);

/**
 * @brief Gives a sample of a channel. It may be called from an interrupt. Outside a sample window it is ignored,
 *        and so are the samples of a window past 65535.
 * @param p_owner Pointer to the owner instance.
 * @param channel Channel, less than channel_count.
 * @param value Sample. The sum of a window is kept in an int32_t.
 */
void pifSensorModulated_sigSample(PifSensorModulated* p_owner, uint8_t channel, int32_t value);

/**
 * @brief Returns one level of a channel from the last cycle.
 * @param p_owner Pointer to the owner instance.
 * @param channel Channel, less than channel_count.
 * @param level Level to return.
 * @return The level, or 0 for a channel out of range.
 */
int32_t pifSensorModulated_GetLevel(PifSensorModulated* p_owner, uint8_t channel, PifSensorModulatedLevel level);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_MODULATED_H
