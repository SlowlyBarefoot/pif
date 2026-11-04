// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_servo.h
 * @brief RC servo driver that maps angles to pulse widths and moves them over time.
 *
 * One PifServo drives up to PIF_SERVO_MAX_COUNT RC servos through one act_control callback, which gets the pulse
 * width in microseconds. The pulse itself is left to a hardware timer of the platform, since a software PWM on the
 * tick cannot resolve the 10 us or so that a degree takes. Angles are in 0.1 degrees.
 */
#ifndef PIF_SERVO_H
#define PIF_SERVO_H


#include "core/pif_task_manager.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_SERVO)
	#define COLLECT_SIGNAL_SERVO	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_SERVO is too
	#include "monitor/pif_collect_signal.h"
#endif


/// Channels one instance takes at most, up to 255.
#ifndef PIF_SERVO_MAX_COUNT
#define PIF_SERVO_MAX_COUNT				16
#endif

#if PIF_SERVO_MAX_COUNT > 255
#error "PIF_SERVO_MAX_COUNT is limited to 255, the range of the channel index"
#endif

/// Pulse in microseconds at the lowest angle of a channel after pifServo_Init().
#ifndef PIF_SERVO_DEFAULT_MIN_PULSE
#define PIF_SERVO_DEFAULT_MIN_PULSE		1000
#endif

/// Pulse in microseconds at the highest angle of a channel after pifServo_Init().
#ifndef PIF_SERVO_DEFAULT_MAX_PULSE
#define PIF_SERVO_DEFAULT_MAX_PULSE		2000
#endif

/// Lowest angle of a channel after pifServo_Init(), in 0.1 degrees.
#ifndef PIF_SERVO_DEFAULT_MIN_ANGLE
#define PIF_SERVO_DEFAULT_MIN_ANGLE		0
#endif

/// Highest angle of a channel after pifServo_Init(), in 0.1 degrees.
#ifndef PIF_SERVO_DEFAULT_MAX_ANGLE
#define PIF_SERVO_DEFAULT_MAX_ANGLE		1800
#endif

/// An angle of pifServo_MoveSync() that leaves its channel as it is.
#define PIF_SERVO_KEEP					INT16_MIN

// The signal name of a channel, "SVP" and an index of up to 3 digits, has to fit in a collect signal channel.
#if defined(COLLECT_SIGNAL_SERVO) && PIF_COLLECT_SIGNAL_NAME_SIZE < 7
#error "PIF_COLLECT_SIGNAL_NAME_SIZE must be at least 7 for pifServo"
#endif


/**
 * @brief How a move goes from its start to its target angle.
 */
typedef enum EnPifServoProfile
{
	SVP_LINEAR		= 0,	///< Constant speed
	SVP_EASE		= 1		///< Starts and ends slowly (smoothstep), which spares the gears and the supply
} PifServoProfile;


struct StPifServo;
typedef struct StPifServo PifServo;

/**
 * @brief Sets the pulse of one servo.
 *
 * index is the channel and pulse_us the pulse width in microseconds, or 0 to stop the pulse so that the servo goes
 * limp. The platform keeps repeating the pulse at the frame rate of its servos, usually 50 Hz.
 */
typedef void (*PifActServoControl)(uint8_t index, uint16_t pulse_us);

/**
 * @brief Reports that a channel reached the target of its move.
 */
typedef void (*PifEvtServo)(PifServo *p_owner, uint8_t index);

/**
 * @brief Keeps the other context out of the instance, by masking its interrupt for example.
 * @param p_owner Servo instance.
 * @param lock TRUE to enter, FALSE to leave.
 */
typedef void (*PifActServoLock)(PifServo *p_owner, BOOL lock);


#ifdef COLLECT_SIGNAL_SERVO

/**
 * @brief Signals of a channel for pifServo_SetCsFlag() and pifServo_ResetCsFlag().
 *
 * The _IDX values index the signals of a channel, the _BIT values select them.
 */
typedef enum EnPifServoCsFlag
{
    SV_CSF_OFF			= 0,

    SV_CSF_PULSE_IDX	= 0,

	SV_CSF_PULSE_BIT	= 1,
	SV_CSF_ALL_BIT		= 1,

    SV_CSF_COUNT		= 1
} PifServoCsFlag;

#endif	// COLLECT_SIGNAL_SERVO

/**
 * @class StPifServoChannel
 * @brief State of one servo of an instance.
 */
typedef struct StPifServoChannel
{
	// Read-only Member Variable
	uint16_t _min_pulse;		///< Pulse at _min_angle in microseconds, before the trim
	uint16_t _max_pulse;		///< Pulse at _max_angle in microseconds, before the trim
	int16_t _min_angle;			///< Lowest angle in 0.1 degrees
	int16_t _max_angle;			///< Highest angle in 0.1 degrees
	int16_t _trim;				///< Added to every pulse, in microseconds
	BOOL _reverse;				///< _min_angle takes _max_pulse and _max_angle _min_pulse
	PifServoProfile _profile;	///< Profile of the moves
	uint16_t _pulse;			///< Pulse given last to act_control, 0 while detached
	int16_t _angle;				///< Angle of _pulse. Only meaningful while attached.

	// Private Member Variable
	BOOL __moving;
	PifServoProfile __profile;	///< Profile of the move under way
	int16_t __start_angle;
	int16_t __target_angle;
	uint32_t __move_start;		///< On the microsecond clock
	uint16_t __move_time;		///< In milliseconds
#ifdef COLLECT_SIGNAL_SERVO
	PifCollectSignalChannel __cs[SV_CSF_COUNT];
#endif
} PifServoChannel;

/**
 * @class StPifServo
 * @brief Up to PIF_SERVO_MAX_COUNT RC servos. One TM_EXTERNAL task updates the pulses of the channels that move,
 *        every period while any of them does, and sleeps while none does. Commands may come from an interrupt
 *        once a lock is attached with pifServo_AttachActLock().
 */
struct StPifServo
{
	// Public Event Function
    PifEvtServo evt_arrive;	///< Called when a channel reaches the target of a move. May be NULL.

    // Read-only Member Variable
    PifId _id;				///< Instance ID
    uint8_t _count;			///< Number of channels
    uint32_t _period;		///< Update period of a move in microseconds
    PifTask *_p_task;		///< Task that updates the moves

	// Private Member Variable
    PifServoChannel *__p_channel;

    // Private Action Function
    PifActServoControl __act_control;
    PifActServoLock __act_lock;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an instance of count servos and adds the task that moves them, so pifTaskManager_Init() must
 *        have room for one more task. No pulse is given until a channel is set, so the servos keep still at
 *        power up. Every channel starts with the PIF_SERVO_DEFAULT_ range and the SVP_LINEAR profile.
 * @param p_owner Target servo instance to initialize.
 * @param id Instance ID. If set to PIF_ID_AUTO, an ID is assigned automatically.
 * @param count Number of servos, 1 to PIF_SERVO_MAX_COUNT.
 * @param period_ms Update period of a move in milliseconds, greater than 0. The frame of the servos, usually
 *        20 ms, is the natural choice: a shorter one gives pulses the servo never sees.
 * @param act_control Hardware control callback that sets the pulse of the channel at index.
 * @return TRUE if initialization succeeds; otherwise FALSE.
 */
BOOL pifServo_Init(PifServo *p_owner, PifId id, uint8_t count, uint16_t period_ms, PifActServoControl act_control);

/**
 * @brief Detaches every channel that is attached and releases the resources allocated by pifServo_Init.
 * @param p_owner Servo instance to clear.
 */
void pifServo_Clear(PifServo *p_owner);

/**
 * @brief Sets the angle range of one channel and the pulses at its ends. A move under way stops, and an attached
 *        channel is given the pulse of its angle in the new range, held within it.
 * @param p_owner Servo instance to configure.
 * @param index Zero-based channel index, less than count.
 * @param min_pulse Pulse at min_angle in microseconds, greater than the size of the trim.
 * @param max_pulse Pulse at max_angle in microseconds, greater than min_pulse.
 * @param min_angle Lowest angle in 0.1 degrees, greater than PIF_SERVO_KEEP.
 * @param max_angle Highest angle in 0.1 degrees, greater than min_angle.
 * @return TRUE if the range is accepted; otherwise FALSE.
 */
BOOL pifServo_SetRange(PifServo *p_owner, uint8_t index, uint16_t min_pulse, uint16_t max_pulse, int16_t min_angle,
		int16_t max_angle);

/**
 * @brief Sets an offset added to every pulse of one channel, to line up the horn of a servo with its angle. An
 *        attached channel is given the new pulse of its angle at once.
 * @param p_owner Servo instance to configure.
 * @param index Zero-based channel index, less than count.
 * @param trim Offset in microseconds, smaller in size than the pulse at the lowest angle.
 * @return TRUE if the trim is accepted; otherwise FALSE.
 */
BOOL pifServo_SetTrim(PifServo *p_owner, uint8_t index, int16_t trim);

/**
 * @brief Swaps the pulses of the ends of one channel, for a servo mounted the other way round. An attached
 *        channel is given the new pulse of its angle at once.
 * @param p_owner Servo instance to configure.
 * @param index Zero-based channel index, less than count.
 * @param reverse TRUE to swap them.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifServo_SetReverse(PifServo *p_owner, uint8_t index, BOOL reverse);

/**
 * @brief Sets the profile of the moves of one channel, from the next move on.
 * @param p_owner Servo instance to configure.
 * @param index Zero-based channel index, less than count.
 * @param profile Profile of the moves.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifServo_SetProfile(PifServo *p_owner, uint8_t index, PifServoProfile profile);

/**
 * @brief Gives one channel the pulse of an angle at once, attaching it if it is not. A move under way stops
 *        without evt_arrive.
 * @param p_owner Servo instance to control.
 * @param index Zero-based channel index, less than count.
 * @param angle Angle in 0.1 degrees, held within the range of the channel.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifServo_SetAngle(PifServo *p_owner, uint8_t index, int16_t angle);

/**
 * @brief Gives one channel a pulse as it is, attaching it if it is not, to find the range of a servo or to drive
 *        a continuous rotation servo. The trim and the reverse are not applied to it, and _angle becomes the angle
 *        whose pulse it is, held within the range, so that a move after it starts from there. A move under way
 *        stops without evt_arrive.
 * @param p_owner Servo instance to control.
 * @param index Zero-based channel index, less than count.
 * @param pulse_us Pulse in microseconds, greater than 0. pifServo_Detach() stops the pulse.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifServo_SetPulse(PifServo *p_owner, uint8_t index, uint16_t pulse_us);

/**
 * @brief Moves one channel from its angle now to a target in the time given, and calls evt_arrive when it is
 *        there. A move under way is replaced, starting from where it got to. A channel that is detached does not
 *        know where its servo is, so it is attached at the target at once and evt_arrive follows at the next
 *        update, as for a time of 0.
 * @param p_owner Servo instance to control.
 * @param index Zero-based channel index, less than count.
 * @param angle Target angle in 0.1 degrees, held within the range of the channel.
 * @param time_ms Time of the move in milliseconds. With 0 the channel jumps at the next update.
 * @return TRUE if the move is accepted; otherwise FALSE.
 */
BOOL pifServo_MoveTo(PifServo *p_owner, uint8_t index, int16_t angle, uint16_t time_ms);

/**
 * @brief Moves one channel to a target at a speed, as pifServo_MoveTo() with the time that the distance takes at
 *        that speed. With SVP_EASE the speed is the average one. A move longer than 65535 ms is refused.
 * @param p_owner Servo instance to control.
 * @param index Zero-based channel index, less than count.
 * @param angle Target angle in 0.1 degrees, held within the range of the channel.
 * @param speed Speed in degrees per second, greater than 0.
 * @return TRUE if the move is accepted; otherwise FALSE.
 */
BOOL pifServo_MoveAtSpeed(PifServo *p_owner, uint8_t index, int16_t angle, uint16_t speed);

/**
 * @brief Moves every channel to its target in the same time, so that they start and arrive together, as one
 *        pifServo_MoveTo() for each of them. evt_arrive is called for each channel.
 * @param p_owner Servo instance to control.
 * @param p_angles Target angle of every channel, count of them. PIF_SERVO_KEEP leaves a channel as it is.
 * @param time_ms Time of the moves in milliseconds.
 * @return TRUE if the moves are accepted; otherwise FALSE.
 */
BOOL pifServo_MoveSync(PifServo *p_owner, const int16_t *p_angles, uint16_t time_ms);

/**
 * @brief Stops the move of one channel where it got to, without evt_arrive. The servo holds that angle.
 * @param p_owner Servo instance to control.
 * @param index Zero-based channel index, less than count.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifServo_Stop(PifServo *p_owner, uint8_t index);

/**
 * @brief Stops the pulse of one channel, so that the servo goes limp, and stops its move without evt_arrive.
 * @param p_owner Servo instance to control.
 * @param index Zero-based channel index, less than count.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifServo_Detach(PifServo *p_owner, uint8_t index);

/**
 * @brief Gives the instance a way to keep the other context out. With it, the functions that set or move a
 *        channel may be called from an interrupt that the lock masks, while the task of the instance runs in the
 *        main context. Those calls and the task do their work under the lock, act_control and evt_arrive
 *        included, so keep the callbacks short. An evt_arrive that calls back into the instance needs a lock
 *        that nests. Without a lock no locking is done.
 * @param p_owner Servo instance to configure.
 * @param act_lock Enters and leaves the locked section, or NULL to remove it.
 */
void pifServo_AttachActLock(PifServo *p_owner, PifActServoLock act_lock);

/**
 * @brief Gives the state of one channel, to read its range, trim, reverse, profile and pulse.
 * @param p_owner Servo instance to read.
 * @param index Zero-based channel index, less than count.
 * @return The channel, or NULL for an index out of range.
 */
const PifServoChannel *pifServo_GetChannel(PifServo *p_owner, uint8_t index);

/**
 * @brief Tells the angle of one channel, where its move got to at the last update.
 * @param p_owner Servo instance to read.
 * @param index Zero-based channel index, less than count.
 * @return Angle in 0.1 degrees. 0 for an index out of range, and not meaningful while detached.
 */
int16_t pifServo_GetAngle(PifServo *p_owner, uint8_t index);

/**
 * @brief Tells whether one channel has a move under way.
 * @param p_owner Servo instance to read.
 * @param index Zero-based channel index, less than count.
 * @return TRUE until the move reaches its target, evt_arrive included. FALSE otherwise, or for an index out of
 *         range.
 */
BOOL pifServo_IsMoving(PifServo *p_owner, uint8_t index);

/**
 * @brief Tells whether one channel is given a pulse.
 * @param p_owner Servo instance to read.
 * @param index Zero-based channel index, less than count.
 * @return TRUE while attached. FALSE otherwise, or for an index out of range.
 */
BOOL pifServo_IsAttached(PifServo *p_owner, uint8_t index);


#ifdef COLLECT_SIGNAL_SERVO

/**
 * @brief Adds the selected signals of a channel, or of every channel, to pifCollectSignal. Channel n gets
 *        SVP\<n\>_\<id\>, its pulse in 16 bits, where \<id\> is the instance ID in hex. On failure none of the
 *        selected signals is left added.
 * @param p_owner Pointer to the owner instance.
 * @param index Channel of the instance; PIF_CS_INDEX_ALL or any value from its count on for all.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifServo_SetCsFlag(PifServo *p_owner, uint8_t index, PifServoCsFlag flag);

/**
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param index Channel of the instance; PIF_CS_INDEX_ALL or any value from its count on for all.
 * @param flag Bit mask of the signals to remove.
 */
void pifServo_ResetCsFlag(PifServo *p_owner, uint8_t index, PifServoCsFlag flag);

#endif	// COLLECT_SIGNAL_SERVO

#ifdef __cplusplus
}
#endif


#endif  // PIF_SERVO_H
