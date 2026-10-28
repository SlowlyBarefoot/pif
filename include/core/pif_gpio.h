// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_gpio.h
 * @brief Group of GPIO pins read or written as one bit field.
 *
 * The platform reads and writes the pins of a group through two callbacks. Input changes are found by a polling
 * task or reported with pifGpio_sigData(), and come to the application through evt_in.
 */
#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "core/pif_task_manager.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_GPIO)
	#define COLLECT_SIGNAL_GPIO	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_GPIO is too
	#include "core/pif_collect_signal.h"
#endif


/// State width in bits, which is also the maximum pin count of one instance.
#ifndef PIF_GPIO_WIDTH
#define PIF_GPIO_WIDTH		8
#endif

#if PIF_GPIO_WIDTH == 8
typedef uint8_t PifGpioState;
#elif PIF_GPIO_WIDTH == 16
typedef uint16_t PifGpioState;
#elif PIF_GPIO_WIDTH == 32
typedef uint32_t PifGpioState;
#else
#error "PIF_GPIO_WIDTH must be 8, 16 or 32"
#endif

#define PIF_GPIO_MAX_COUNT		PIF_GPIO_WIDTH	///< Most pins of one instance


struct StPifGpio;
typedef struct StPifGpio PifGpio;

/**
 * @brief Reads all input pins of the instance id. Bit i is pin i.
 */
typedef PifGpioState (*PifActGpioIn)(PifId id);
/**
 * @brief Writes all output pins of the instance id. Bit i is pin i.
 */
typedef void (*PifActGpioOut)(PifId id, PifGpioState state);

/**
 * @brief Reports that input pin index changed to state.
 */
typedef void (*PifEvtGpioIn)(PifGpio* p_owner, uint8_t index, SWITCH state);


#ifdef COLLECT_SIGNAL_GPIO

/**
 * @brief Signals of a gpio for pifGpio_SetCsFlag() and pifGpio_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifGpioCsFlag
{
    GP_CSF_OFF			= 0,

	GP_CSF_STATE_IDX	= 0,

	GP_CSF_STATE_BIT	= 1,
	GP_CSF_ALL_BIT		= 1,

	GP_CSF_COUNT		= 1
} PifGpioCsFlag;

#endif	// COLLECT_SIGNAL_GPIO

/**
 * @class StPifGpio
 * @brief Groups up to PIF_GPIO_MAX_COUNT pins that are read or written as one bit field.
 *
 * Input and output callbacks are independent, so one instance may use either or both. Bit i of a state is pin i.
 * evt_in reports input changes found by the polling task or by pifGpio_sigData.
 */
struct StPifGpio
{
	// Public Member Variable
    uint8_t count;	///< Number of pins in use, 1 to PIF_GPIO_MAX_COUNT

	// Public Event Function
	PifEvtGpioIn evt_in;	///< Called for every input pin that changed. May be NULL.

	// Read-only Member Variable
	PifId _id;	///< Instance ID

	// Private Member Variable
	PifGpioState __read_state;	///< Last input state, the reference for evt_in.
	PifGpioState __write_state;

#ifdef COLLECT_SIGNAL_GPIO
	PifCollectSignalChannel __cs[GP_CSF_COUNT];
#endif

	// Private Action Function
	PifActGpioIn __act_in;
	PifActGpioOut __act_out;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a group of count pins. Attach the read or write callback next.
 * @param p_owner Pointer to the target object instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param count Number of pins, 1 to PIF_GPIO_MAX_COUNT.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifGpio_Init(PifGpio* p_owner, PifId id, uint8_t count);

/**
 * @brief Removes the collected signals of the group.
 * @param p_owner Pointer to the target object instance.
 */
void pifGpio_Clear(PifGpio* p_owner);

/**
 * @brief Reads all pins through the input callback.
 * @param p_owner Pointer to the target object instance.
 * @return Pin states masked to count bits. On failure 0 with pif_error set to E_CANNOT_USE.
 */
PifGpioState pifGpio_ReadAll(PifGpio* p_owner);

/**
 * @brief Reads one pin through the input callback.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based pin index, less than count.
 * @return Pin state. On failure OFF with pif_error set.
 */
SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index);

/**
 * @brief Writes all pins through the output callback.
 * @param p_owner Pointer to the target object instance.
 * @param state Pin states. Bits at or above count are ignored.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifGpio_WriteAll(PifGpio* p_owner, PifGpioState state);

/**
 * @brief Changes one pin and writes all pins through the output callback.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based pin index, less than count.
 * @param state Pin state.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifGpio_WriteCell(PifGpio* p_owner, uint8_t index, SWITCH state);

/**
 * @brief Reports one input pin state, e.g. from a pin interrupt. Calls evt_in when the state changed.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based pin index, less than count.
 * @param state Pin state.
 */
void pifGpio_sigData(PifGpio* p_owner, uint8_t index, SWITCH state);

/**
 * @brief Attaches the callback that reads all input pins.
 * @param p_owner Pointer to the target object instance.
 * @param act_in Input callback function to read GPIO state.
 */
void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in);

/**
 * @brief Attaches the callback that writes all output pins.
 * @param p_owner Pointer to the target object instance.
 * @param act_out Output callback function to write GPIO state.
 */
void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out);

/**
 * @brief Adds a task that polls the input callback and calls evt_in for every changed pin.
 *        The current input state becomes the reference, so pins already set do not raise events.
 * @param p_owner Pointer to the target object instance.
 * @param id Task ID, or PIF_ID_AUTO.
 * @param mode Mode of the task, usually TM_PERIOD.
 * @param period Execution period value for scheduling.
 * @param start Set to TRUE to start immediately after configuration.
 * @return The task, or NULL with pif_error set.
 */
PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifId id, PifTaskMode mode, uint16_t period, BOOL start);


#ifdef COLLECT_SIGNAL_GPIO

/**
 * @brief Adds the selected signals of the gpio to pifCollectSignal as channels.
 * @param p_owner Pointer to the target object instance.
 * @param index Device of the instance for a module that controls several, as PifSolenoid; PIF_CS_INDEX_ALL
 *        or any value from its count on for all. Ignored by a module of one device.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE. A channel holds at most 32 bits, so FALSE with E_INVALID_PARAM
 *         when count is above 32.
 */
BOOL pifGpio_SetCsFlag(PifGpio* p_owner, uint8_t index, PifGpioCsFlag flag);

/**
 * @brief Removes the selected signals of the gpio from pifCollectSignal.
 * @param p_owner Pointer to the target object instance.
 * @param index Device of the instance for a module that controls several, as PifSolenoid; PIF_CS_INDEX_ALL
 *        or any value from its count on for all. Ignored by a module of one device.
 * @param flag Bit mask of the signals to remove.
 */
void pifGpio_ResetCsFlag(PifGpio* p_owner, uint8_t index, PifGpioCsFlag flag);

#endif	// COLLECT_SIGNAL_GPIO

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
