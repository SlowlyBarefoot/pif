// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "core/pif_task_manager.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


#define PIF_GPIO_MAX_COUNT		7


struct StPifGpio;
typedef struct StPifGpio PifGpio;

typedef uint8_t (*PifActGpioIn)(PifId id);
typedef void (*PifActGpioOut)(PifId id, uint8_t state);

typedef void (*PifEvtGpioIn)(uint8_t index, uint8_t state);


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifGpioCsFlag
{
    GP_CSF_OFF			= 0,

	GP_CSF_STATE_IDX	= 0,

	GP_CSF_STATE_BIT	= 1,
	GP_CSF_ALL_BIT		= 1,

	GP_CSF_COUNT		= 1
} PifGpioCsFlag;

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifGpio
 * @brief Provides a type or declaration used by this module.
 */
struct StPifGpio
{
	// Public Member Variable
    uint8_t count;

	// Public Event Function
	PifEvtGpioIn evt_in;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	uint8_t __read_state;
	uint8_t __write_state;

#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[GP_CSF_COUNT];
#endif

	// Private Action Function
	union {
		PifActGpioIn act_in;
		PifActGpioOut act_out;
	} __ui;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGpio_Init
 * @brief Initializes the gpio instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 * @param count Number of items or channels.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifGpio_Init(PifGpio* p_owner, PifId id, uint8_t count);

/**
 * @fn pifGpio_Clear
 * @brief Clears the gpio state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifGpio_Clear(PifGpio* p_owner);

/**
 * @fn pifGpio_ReadAll
 * @brief Executes the pifGpio_ReadAll operation for the gpio module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint8_t pifGpio_ReadAll(PifGpio* p_owner);

/**
 * @fn pifGpio_ReadCell
 * @brief Executes the pifGpio_ReadCell operation for the gpio module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based index of the target item.
 * @return Return value of this API.
 */
SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index);

/**
 * @fn pifGpio_WriteAll
 * @brief Executes the pifGpio_WriteAll operation for the gpio module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param state Target state value to apply.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifGpio_WriteAll(PifGpio* p_owner, uint8_t state);

/**
 * @fn pifGpio_WriteCell
 * @brief Executes the pifGpio_WriteCell operation for the gpio module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param ucIndex Zero-based index of the target item.
 * @param swState Switch state value to write or compare.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifGpio_WriteCell(PifGpio* p_owner, uint8_t index, SWITCH state);

/**
 * @fn pifGpio_sigData
 * @brief Processes an external signal or tick for the gpio and updates runtime timing state.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based index of the target item.
 * @param state Target state value to apply.
 */
void pifGpio_sigData(PifGpio* p_owner, uint8_t index, SWITCH state);

/**
 * @fn pifGpio_AttachActIn
 * @brief Attaches a callback, device, or external resource to the gpio for integration.
 * @param p_owner Pointer to the target object instance.
 * @param act_in Input callback function to read GPIO state.
 */
void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in);

/**
 * @fn pifGpio_AttachActOut
 * @brief Attaches a callback, device, or external resource to the gpio for integration.
 * @param p_owner Pointer to the target object instance.
 * @param act_out Output callback function to write GPIO state.
 */
void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out);

/**
 * @fn pifGpio_AttachTaskIn
 * @brief Attaches a callback, device, or external resource to the gpio for integration.
 * @param p_owner Pointer to the target object instance.
 * @param mode Operating mode configuration value.
 * @param id Identifier value for the object or task.
 * @param period Execution period value for scheduling.
 * @param start Set to TRUE to start immediately after configuration.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifId id, PifTaskMode mode, uint16_t period, BOOL start);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifGpio_SetCsFlag
 * @brief Adds the selected signals of the gpio to pifCollectSignal as channels.
 * @param p_owner Pointer to the target object instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifGpio_SetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag);

/**
 * @fn pifGpio_ResetCsFlag
 * @brief Removes the selected signals of the gpio from pifCollectSignal.
 * @param p_owner Pointer to the target object instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifGpio_ResetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
