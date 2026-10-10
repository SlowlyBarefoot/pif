// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SOLENOID_H
#define PIF_SOLENOID_H


#include "core/pif_ring_data.h"
#include "core/pif_task_manager.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


// Channels one instance takes at most, up to 255.
#ifndef PIF_SOLENOID_MAX_COUNT
#define PIF_SOLENOID_MAX_COUNT		16
#endif

#if PIF_SOLENOID_MAX_COUNT > 255
#error "PIF_SOLENOID_MAX_COUNT is limited to 255, the range of the channel index"
#endif

// The signal names of a channel, "SNA" or "SND" and an index of up to 3 digits, have to fit in a
// collect signal channel.
#if defined(PIF_COLLECT_SIGNAL) && PIF_COLLECT_SIGNAL_NAME_SIZE < 7
#error "PIF_COLLECT_SIGNAL_NAME_SIZE must be at least 7 for pifSolenoid"
#endif


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

typedef void (*PifActSolenoidControl)(uint8_t index, SWITCH action, PifSolenoidDir dir);

typedef void (*PifEvtSolenoid)(PifSolenoid* p_owner, uint8_t index);


/**
 * @class StPifSolenoidContent
 * @brief Queued solenoid command item used by the internal ring buffer.
 */
typedef struct StPifSolenoidContent
{
	uint32_t delay;				// In microseconds, from the command queued before it
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

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifSolenoidChannel
 * @brief State of one solenoid of an instance.
 */
typedef struct StPifSolenoidChannel
{
	// Public Member Variable
    uint16_t on_time;			// ON pulse duration in milliseconds, 0 for no automatic OFF

	// Private Member Variable
    BOOL __state;
    PifSolenoidDir __current_dir;
    BOOL __on_pending;			// The pulse is timed and __on_end is when it ends
    uint32_t __on_end;
    BOOL __delay_pending;		// A delayed command waits until __delay_end, in __dir
    uint32_t __delay_end;
    PifSolenoidDir __dir;
	PifRingData* __p_buffer;
#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[SN_CSF_COUNT];
#endif
} PifSolenoidChannel;

/**
 * @class StPifSolenoid
 * @brief Up to PIF_SOLENOID_MAX_COUNT solenoids, each with a timed ON pulse and optional command
 *        buffering. One TM_EXTERNAL task does the timing of all of them, woken by
 *        pifTask_SetTrigger() at the earliest end of a pulse or delayed command among the channels.
 *        It runs in the main context and, with a TM_REALTIME task, waits for the realtime slack
 *        like any other trigger.
 */
struct StPifSolenoid
{
	// Public Event Function
    PifEvtSolenoid evt_off;
    PifEvtSolenoid evt_error;

    // Read-only Member Variable
    PifId _id;
    PifSolenoidType _type;
    uint8_t _count;
    PifTask* _p_task;

	// Private Member Variable
    PifSolenoidChannel* __p_channel;

    // Private Action Function
    PifActSolenoidControl __act_control;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSolenoid_Init
 * @brief Initializes an instance of count solenoids and adds the task that times them, so
 *        pifTaskManager_Init() must have room for one more task.
 * @param p_owner Target solenoid instance to initialize.
 * @param id Instance ID. If set to PIF_ID_AUTO, an ID is assigned automatically.
 * @param count Number of solenoids, 1 to PIF_SOLENOID_MAX_COUNT.
 * @param type Solenoid type of every channel. ST_2POINT ignores a request in the direction a
 *        channel already holds. ST_1POINT and ST_3POINT behave the same here, the direction is
 *        only passed on to act_control.
 * @param on_time ON pulse duration of every channel in milliseconds. If 0, automatic OFF is
 *        disabled. pifSolenoid_SetOnTime() changes it per channel.
 * @param act_control Hardware control callback that applies ON/OFF action and direction to the
 *        channel at index.
 * @return TRUE if initialization succeeds; otherwise FALSE.
 */
BOOL pifSolenoid_Init(PifSolenoid* p_owner, PifId id, uint8_t count, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control);

/**
 * @fn pifSolenoid_Clear
 * @brief Switches every channel that is ON OFF and releases resources allocated by
 *        pifSolenoid_Init and pifSolenoid_SetBuffer.
 * @param p_owner Solenoid instance to clear.
 */
void pifSolenoid_Clear(PifSolenoid* p_owner);

/**
 * @fn pifSolenoid_SetBuffer
 * @brief Creates a ring buffer for delayed or queued commands of one channel. A buffer set before
 *        is released together with the commands still in it.
 * @param p_owner Solenoid instance to configure.
 * @param index Zero-based channel index, less than count.
 * @param size Number of queued command entries to allocate. Must be greater than 0.
 * @return TRUE if the buffer is created successfully; otherwise FALSE.
 */
BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint8_t index, uint16_t size);

/**
 * @fn pifSolenoid_SetInvalidDirection
 * @brief Resets the current direction state of one channel to SD_INVALID.
 * @param p_owner Solenoid instance to update.
 * @param index Zero-based channel index, less than count.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner, uint8_t index);

/**
 * @fn pifSolenoid_SetOnTime
 * @brief Updates the ON pulse duration of one channel.
 * @param p_owner Solenoid instance to update.
 * @param index Zero-based channel index, less than count.
 * @param on_time ON pulse duration in milliseconds, from the next pulse on. If 0, automatic OFF is
 *        disabled, as in pifSolenoid_Init.
 * @return TRUE if the value is accepted; otherwise FALSE.
 */
BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint8_t index, uint16_t on_time);

/**
 * @fn pifSolenoid_ActionOn
 * @brief Turns one channel ON immediately or after a delay, without a direction.
 * @param p_owner Solenoid instance to control.
 * @param index Zero-based channel index, less than count.
 * @param delay Delay before activation in milliseconds.
 * @return TRUE if the command is accepted; otherwise FALSE. Always FALSE for ST_2POINT, which needs
 *         a direction: use pifSolenoid_ActionOnDir.
 */
BOOL pifSolenoid_ActionOn(PifSolenoid* p_owner, uint8_t index, uint16_t delay);

/**
 * @fn pifSolenoid_ActionOnDir
 * @brief Turns one channel ON with the specified direction, immediately or after a delay.
 * @param p_owner Solenoid instance to control.
 * @param index Zero-based channel index, less than count.
 * @param delay Delay before activation in milliseconds. While a delayed command of the channel is
 *        waiting, it is counted from now and the command is queued if the channel has a buffer.
 * @param dir Direction to apply when activating the solenoid. SD_INVALID is rejected for ST_2POINT.
 * @return TRUE if the command is accepted; otherwise FALSE. Accepted is not executed: for
 *         ST_2POINT, a command in the direction already held is skipped when it comes due.
 */
BOOL pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint8_t index, uint16_t delay, PifSolenoidDir dir);

/**
 * @fn pifSolenoid_ActionOff
 * @brief Forces one channel OFF immediately, stops timing its pulse and cancels its delayed and
 *        queued commands, so that none of them switches it back on.
 * @param p_owner Solenoid instance to control.
 * @param index Zero-based channel index, less than count.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifSolenoid_ActionOff(PifSolenoid* p_owner, uint8_t index);

/**
 * @fn pifSolenoid_IsOn
 * @brief Tells whether one channel is ON.
 * @param p_owner Solenoid instance to read.
 * @param index Zero-based channel index, less than count.
 * @return TRUE while the channel is ON. FALSE otherwise, or for an index out of range.
 */
BOOL pifSolenoid_IsOn(PifSolenoid* p_owner, uint8_t index);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifSolenoid_SetCsFlag
 * @brief Adds the selected signals of every channel to pifCollectSignal. Channel n gets
 *        SNA<n>_<id>, its state in 1 bit, and SND<n>_<id>, its direction in 2 bits, where <id> is
 *        the instance ID in hex. On failure none of the selected signals is left added.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSolenoid_SetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoid_ResetCsFlag
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifSolenoid_ResetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
