#ifndef PIF_COLLECT_SIGNAL_H
#define PIF_COLLECT_SIGNAL_H


#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"


#ifdef PIF_COLLECT_SIGNAL

#define PIF_COLLECT_SIGNAL_GET_FLAG(FLAG, IDX)	(((FLAG)[(IDX) / 2] >> (((IDX) & 1) * 4)) & 0x0F)


typedef enum EnPifCollectSignalScale
{
	CSS_1S				= 0,
	CSS_1MS				= 1,	// Default
	CSS_1US				= 2,
#if 0
	CSS_1NS				= 3,	// Not support
	CSS_1PS				= 4,	// Not support
	CSS_1FS				= 5		// Not support
#endif
} PifCollectSignalScale;

typedef enum EnPifCollectSignalFlag
{
	CSF_GPIO			= 0,
	CSF_SENSOR_DIGITAL	= 1,
	CSF_SENSOR_SWITCH	= 2,
	CSF_SEQUENCE		= 3,
	CSF_SOLENOID		= 4,
	CSF_PULSE			= 5,
	CSF_MAX31855		= 6
} PifCollectSignalFlag;

typedef enum EnPifCollectSignalMethod
{
	CSM_LOG				= 0,
	CSM_BUFFER			= 1
} PifCollectSignalMethod;

typedef enum EnPifCollectSignalVarType
{
	CSVT_EVENT			= 0,	// Not support
	CSVT_INTEGER		= 1,
	CSVT_PARAMETER		= 2,	// Not support
	CSVT_REAL			= 3,
	CSVT_REG			= 4,
	CSVT_SUPPLY0		= 5,	// Not support
	CSVT_SUPPLY1		= 6,	// Not support
	CSVT_TIME			= 7,	// Not support
	CSVT_TRI			= 8,	// Not support
	CSVT_TRIAND			= 9,	// Not support
	CSVT_TRIOR			= 10,	// Not support
	CSVT_TRIREG			= 11,	// Not support
	CSVT_TRI0			= 12,	// Not support
	CSVT_TRI1			= 13,	// Not support
	CSVT_WAND			= 14,	// Not support
	CSVT_WIRE			= 15,
	CSVT_WOR			= 16	// Not support
} PifCollectSignalVarType;


typedef void (*PifAddCollectSignalDevice)();


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifCollectSignal_Init
 * @brief Initializes the collect signal instance and prepares all internal fields for safe use.
 * @param p_module_name Module name string for logging or registration.
 */
void pifCollectSignal_Init(const char* p_module_name);

/**
 * @fn pifCollectSignal_InitHeap
 * @brief Initializes the collect signal with heap memory allocation and default runtime state.
 * @param p_module_name Module name string for logging or registration.
 * @param size Size value used for allocation or capacity.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_InitHeap(const char* p_module_name, uint16_t size);

/**
 * @fn pifCollectSignal_InitStatic
 * @brief Initializes the collect signal with caller-provided static memory and default runtime state.
 * @param p_module_name Module name string for logging or registration.
 * @param size Size value used for allocation or capacity.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_InitStatic(const char* p_module_name, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifCollectSignal_Clear
 * @brief Clears the collect signal state and releases resources currently owned by the instance.
 */
void pifCollectSignal_Clear();

/**
 * @fn pifCollectSignal_GetTransferPeriod
 * @brief Retrieves the requested value or pointer from the collect signal without changing ownership.
 * @return Result value returned by this API.
 */
uint16_t pifCollectSignal_GetTransferPeriod();

/**
 * @fn pifCollectSignal_SetTransferPeriod
 * @brief Sets configuration or runtime state for the collect signal based on the provided parameters.
 * @param period1ms Transfer period in milliseconds.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_SetTransferPeriod(uint16_t period1ms);

/**
 * @fn pifCollectSignal_ChangeScale
 * @brief Changes runtime configuration of the collect signal while preserving object ownership semantics.
 * @param scale Scale factor applied to collected values.
 */
BOOL pifCollectSignal_ChangeScale(PifCollectSignalScale scale);

/**
 * @fn pifCollectSignal_ChangeFlag
 * @brief Changes runtime configuration of the collect signal while preserving object ownership semantics.
 * @param p_flag Pointer to the flag field to modify.
 * @param index Zero-based index of the target item.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifCollectSignal_ChangeFlag(uint8_t* p_flag, uint8_t index, uint8_t flag);

/**
 * @fn pifCollectSignal_ChangeMethod
 * @brief Changes runtime configuration of the collect signal while preserving object ownership semantics.
 * @param method Collection or processing method selector.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_ChangeMethod(PifCollectSignalMethod method);

/**
 * @fn pifCollectSignal_Attach
 * @brief Attaches a callback, device, or external resource to the collect signal for integration.
 * @param flag Bit flag mask to set, clear, or query.
 * @param add_device Callback used to add device metadata entries.
 */
void pifCollectSignal_Attach(PifCollectSignalFlag flag, PifAddCollectSignalDevice add_device);

/**
 * @fn pifCollectSignal_Detach
 * @brief Detaches a callback, device, or external resource from the collect signal.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifCollectSignal_Detach(PifCollectSignalFlag flag);

/**
 * @fn pifCollectSignal_AddDevice
 * @brief Adds an item to the collect signal and updates internal bookkeeping for subsequent operations.
 * @param id Identifier value for the object or task.
 * @param var_type Variable type descriptor for signal registration.
 * @param size Size value used for allocation or capacity.
 * @param p_reference Pointer to the referenced runtime variable.
 * @param initial_value Initial value used when creating the signal entry.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
void* pifCollectSignal_AddDevice(PifId id, PifCollectSignalVarType var_type, uint16_t size,
		const char* p_reference, uint16_t initial_value);

/**
 * @fn pifCollectSignal_Start
 * @brief Starts the collect signal operation using the current timing, trigger, or mode configuration.
 */
void pifCollectSignal_Start();

/**
 * @fn pifCollectSignal_Stop
 * @brief Stops the collect signal operation and transitions it into an inactive state.
 */
void pifCollectSignal_Stop();

/**
 * @fn pifCollectSignal_AddSignal
 * @brief Adds an item to the collect signal and updates internal bookkeeping for subsequent operations.
 * @param p_dev Pointer to the target device descriptor.
 * @param state Target state value to apply.
 */
void pifCollectSignal_AddSignal(void* p_dev, uint16_t state);

/**
 * @fn pifCollectSignal_PrintLog
 * @brief Prints collected diagnostic data from the collect signal in a formatted log representation.
 */
void pifCollectSignal_PrintLog();

/**
 * @fn pifCollectSignal_AttachTask
 * @brief Attaches a callback, device, or external resource to the collect signal for integration.
 * @param mode Operating mode configuration value.
 * @param period Execution period value for scheduling.
 * @param start Set to TRUE to start immediately after configuration.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
PifTask* pifCollectSignal_AttachTask(PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif	// PIF_COLLECT_SIGNAL


#endif	// PIF_COLLECT_SIGNAL_H
