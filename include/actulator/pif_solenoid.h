#ifndef PIF_SOLENOID_H
#define PIF_SOLENOID_H


#include "core/pif_ring_data.h"
#include "core/pif_timer_manager.h"


typedef enum EnPifSolenoidType
{
	ST_1POINT		= 0,
	ST_2POINT		= 1,
	ST_3POINT		= 2
} PifSolenoidType;

typedef enum EnPifSolenoidDir
{
    SD_INVALID		= 0,
    SD_LEFT			= 1,
    SD_RIGHT	    = 2
} PifSolenoidDir;


struct StPifSolenoid;
typedef struct StPifSolenoid PifSolenoid;

typedef void (*PifActSolenoidControl)(SWITCH action, PifSolenoidDir dir);

typedef void (*PifEvtSolenoid)(PifSolenoid* p_owner);


/**
 * @class StPifSolenoidContent
 * @brief Queued solenoid command item used by the internal ring buffer.
 */
typedef struct StPifSolenoidContent
{
	uint16_t delay;
	PifSolenoidDir dir;
} PifSolenoidContent;

#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifSolenoidCsFlag
{
    SN_CSF_OFF			= 0,

    SN_CSF_ACTION_IDX	= 0,
    SN_CSF_DIR_IDX		= 1,

	SN_CSF_ACTION_BIT	= 1,
	SN_CSF_DIR_BIT		= 2,
	SN_CSF_ALL_BIT		= 3,

    SN_CSF_COUNT		= 2
} PifSolenoidCsFlag;

typedef struct StPifSolenoidColSig
{
	PifSolenoid* p_owner;
	uint8_t flag;
    void* p_device[SN_CSF_COUNT];
} PifSolenoidColSig;

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifSolenoid
 * @brief Solenoid actuator object with timer-based ON pulse and optional command buffering.
 */
struct StPifSolenoid
{
	// Public Member Variable
    uint16_t on_time;

	// Public Event Function
    PifEvtSolenoid evt_off;
    PifEvtSolenoid evt_error;

    // Read-only Member Variable
    PifId _id;
    PifSolenoidType _type;

	// Private Member Variable
    PifTimerManager* __p_timer_manager;
    BOOL __state;
    PifSolenoidDir __current_dir;
    PifTimer* __p_timer_on;
	PifTimer* __p_timer_delay;
    PifSolenoidDir __dir;
	PifRingData* __p_buffer;
#ifdef PIF_COLLECT_SIGNAL
	PifSolenoidColSig* __p_colsig;
#endif

    // Private Action Function
    PifActSolenoidControl __act_control;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSolenoid_Init
 * @brief Initializes a solenoid instance and allocates internal timers.
 * @param p_owner Target solenoid instance to initialize.
 * @param id Instance ID. If set to PIF_ID_AUTO, an ID is assigned automatically.
 * @param p_timer_manager Timer manager used to create ON and delay timers.
 * @param type Solenoid type (1-point, 2-point, or 3-point behavior).
 * @param on_time ON pulse duration in milliseconds. If 0, automatic OFF by timer is disabled.
 * @param act_control Hardware control callback that applies ON/OFF action and direction.
 * @return TRUE if initialization succeeds; otherwise FALSE.
 */
BOOL pifSolenoid_Init(PifSolenoid* p_owner, PifId id, PifTimerManager* p_timer_manager, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control);

/**
 * @fn pifSolenoid_Clear
 * @brief Releases resources allocated by pifSolenoid_Init.
 * @param p_owner Solenoid instance to clear.
 */
void pifSolenoid_Clear(PifSolenoid* p_owner);

/**
 * @fn pifSolenoid_SetBuffer
 * @brief Creates a ring buffer for delayed or queued solenoid commands.
 * @param p_owner Solenoid instance to configure.
 * @param size Number of queued command entries to allocate.
 * @return TRUE if the buffer is created successfully; otherwise FALSE.
 */
BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint16_t size);

/**
 * @fn pifSolenoid_SetInvalidDirection
 * @brief Resets the current direction state to SD_INVALID.
 * @param p_owner Solenoid instance to update.
 */
void pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner);

/**
 * @fn pifSolenoid_SetOnTime
 * @brief Updates the ON pulse duration.
 * @param p_owner Solenoid instance to update.
 * @param on_time ON pulse duration in milliseconds. Must be greater than 0.
 * @return TRUE if the value is accepted; otherwise FALSE.
 */
BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint16_t on_time);

/**
 * @fn pifSolenoid_ActionOn
 * @brief Turns the solenoid ON immediately or after a delay.
 * @param p_owner Solenoid instance to control.
 * @param delay Delay before activation in milliseconds.
 * @return TRUE if the value is accepted; otherwise FALSE.
 */
BOOL pifSolenoid_ActionOn(PifSolenoid* p_owner, uint16_t delay);

/**
 * @fn pifSolenoid_ActionOnDir
 * @brief Turns the solenoid ON with the specified direction, immediately or after a delay.
 * @param p_owner Solenoid instance to control.
 * @param delay Delay before activation in milliseconds.
 * @param dir Direction to apply when activating the solenoid.
 * @return TRUE if the value is accepted; otherwise FALSE.
 */
BOOL pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint16_t delay, PifSolenoidDir dir);

/**
 * @fn pifSolenoid_ActionOff
 * @brief Forces the solenoid OFF immediately and stops the running ON timer.
 * @param p_owner Solenoid instance to control.
 */
void pifSolenoid_ActionOff(PifSolenoid* p_owner);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifSolenoid_SetCsFlag
 * @brief Enables selected collect-signal channels for a single solenoid instance.
 * @param p_owner Target solenoid instance.
 * @param flag Bit mask of channels to enable.
 */
void pifSolenoid_SetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoid_ResetCsFlag
 * @brief Disables selected collect-signal channels for a single solenoid instance.
 * @param p_owner Target solenoid instance.
 * @param flag Bit mask of channels to disable.
 */
void pifSolenoid_ResetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoidColSig_Init
 * @brief 
 */
void pifSolenoidColSig_Init();

/**
 * @fn pifSolenoidColSig_Clear
 * @brief 
 */
void pifSolenoidColSig_Clear();

/**
 * @fn pifSolenoidColSig_SetFlag
 * @brief Enables selected collect-signal channels for all registered solenoid instances.
 * @param flag Bit mask of channels to enable.
 */
void pifSolenoidColSig_SetFlag(PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoidColSig_ResetFlag
 * @brief Disables selected collect-signal channels for all registered solenoid instances.
 * @param flag Bit mask of channels to disable.
 */
void pifSolenoidColSig_ResetFlag(PifSolenoidCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
