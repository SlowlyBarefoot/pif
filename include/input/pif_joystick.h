// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_joystick.h
 * @brief Two-axis joystick that turns raw readings into centred, scaled positions and a direction.
 *
 * Where the readings come from is left to the client: act_read gives the raw X and Y, whether they are
 * two ADC channels of a thumbstick such as the KY-023, the registers of an I2C joystick or a nunchuk, or
 * the switches of an arcade stick given as min, centre and max. A reading that arrives on its own, from
 * DMA, an interrupt or a packet, is given with pifJoystick_sigData() instead, without act_read.
 * The switches of an arcade stick are debounced by a median filter (pifNoiseFilterInt16_InitMedian())
 * attached with pifJoystick_AttachFilter(). Everything after the raw value is here:
 *
 *     raw --filter--> filtered raw --min/centre/max--> -PIF_JOYSTICK_RANGE..+PIF_JOYSTICK_RANGE
 *         --dead zone, inversion--> position --hysteresis--> direction
 *
 * X grows to the right and Y grows upwards. An axis that reads the other way round is turned with
 * pifJoystick_SetInvert(). The push button of a thumbstick is a key of a PifKeypad.
 */
#ifndef PIF_JOYSTICK_H
#define PIF_JOYSTICK_H


#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"


#ifndef PIF_JOYSTICK_CONTROL_PERIOD
	#define PIF_JOYSTICK_CONTROL_PERIOD		20		///< Period in ms of the joystick task
#endif

#ifndef PIF_JOYSTICK_RANGE
	#define PIF_JOYSTICK_RANGE				1000	///< Position at full deflection
#endif

/// Position an axis has to reach for the direction to point that way, and the one it has to fall below
/// for the direction to let go of it again. The gap keeps a stick held near the threshold from chattering.
#ifndef PIF_JOYSTICK_DIRECTION_ON
	#define PIF_JOYSTICK_DIRECTION_ON		500
#endif
#ifndef PIF_JOYSTICK_DIRECTION_OFF
	#define PIF_JOYSTICK_DIRECTION_OFF		300
#endif


/**
 * @brief Axes of a joystick.
 */
typedef enum EnPifJoystickAxis
{
	JA_X			= 0,
	JA_Y			= 1,

	JA_COUNT		= 2
} PifJoystickAxis;

/**
 * @brief Bits of a direction. A diagonal is two of them, JD_UP | JD_RIGHT for instance.
 */
typedef enum EnPifJoystickDirection
{
	JD_NONE			= 0,
	JD_LEFT			= 1,
	JD_RIGHT		= 2,
	JD_UP			= 4,
	JD_DOWN			= 8
} PifJoystickDirection;


struct StPifJoystick;
typedef struct StPifJoystick PifJoystick;

/**
 * @brief Reads the raw position.
 * @param p_owner Pointer to the joystick instance.
 * @param p_x Output pointer for the raw X.
 * @param p_y Output pointer for the raw Y.
 */
typedef void (*PifActJoystickRead)(PifJoystick *p_owner, int16_t *p_x, int16_t *p_y);

/**
 * @brief Reports a new position.
 * @param p_owner Pointer to the joystick instance.
 * @param x Position along X, -PIF_JOYSTICK_RANGE to +PIF_JOYSTICK_RANGE.
 * @param y Position along Y, -PIF_JOYSTICK_RANGE to +PIF_JOYSTICK_RANGE.
 */
typedef void (*PifEvtJoystickMove)(PifJoystick *p_owner, int16_t x, int16_t y);

/**
 * @brief Reports a new direction.
 * @param p_owner Pointer to the joystick instance.
 * @param direction Bits of PifJoystickDirection, JD_NONE when the stick is back in the middle.
 */
typedef void (*PifEvtJoystickDirection)(PifJoystick *p_owner, uint8_t direction);


/**
 * @brief Calibration of one axis.
 */
typedef struct StPifJoystickAxisCal
{
	int16_t _min;					///< Raw value at full deflection to the left or down
	int16_t _center;				///< Raw value at rest
	int16_t _max;					///< Raw value at full deflection to the right or up
	BOOL _invert;					///< The axis reads the other way round
} PifJoystickAxisCal;


/**
 * @class StPifJoystick
 * @brief Two-axis joystick context for calibration, scaling and polling.
 */
struct StPifJoystick
{
	// Public Event Function
	PifEvtJoystickMove evt_move;				///< Whenever the position changes. May be NULL.
	PifEvtJoystickDirection evt_direction;		///< Whenever the direction changes. May be NULL.

	// Read-only Member Variable
	PifId _id;									///< Instance ID
	uint16_t _control_period_1ms;				///< PIF_JOYSTICK_CONTROL_PERIOD
	PifTask *_p_task;							///< Joystick task
	PifJoystickAxisCal _cal[JA_COUNT];			///< Calibration of each axis
	uint16_t _dead_zone;						///< Positions within this of the middle read as 0
	int16_t _raw[JA_COUNT];						///< Last raw value of each axis, after the filter
	int16_t _position[JA_COUNT];				///< Last position of each axis
	uint8_t _direction;							///< Bits of PifJoystickDirection

	// Private Member Variable
	PifNoiseFilter *__p_filter[JA_COUNT];
	BOOL __updated;								///< _raw holds a sample that Process has not used yet
	BOOL __sampled;								///< _raw holds a sample at all

	// Private Action Function
	PifActJoystickRead __act_read;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a joystick instance. Both axes span raw_min to raw_max with the centre half-way,
 *        which pifJoystick_SetAxis() and pifJoystick_CalibrateCenter() then refine.
 * @param p_owner Pointer to the joystick instance to initialize.
 * @param id Instance ID. Use `PIF_ID_AUTO` to assign an ID automatically.
 * @param raw_min Smallest raw value, 0 for an ADC.
 * @param raw_max Largest raw value, 4095 for a 12-bit ADC. Must be at least raw_min + 2.
 * @param act_read Reads the raw position for pifJoystick_Process(). NULL when the readings are given
 *                 with pifJoystick_sigData().
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifJoystick_Init(PifJoystick *p_owner, PifId id, int16_t raw_min, int16_t raw_max, PifActJoystickRead act_read);

/**
 * @brief Releases the task of the joystick instance.
 * @param p_owner Pointer to the joystick instance.
 */
void pifJoystick_Clear(PifJoystick *p_owner);

/**
 * @brief Sets the calibration of one axis.
 * @param p_owner Pointer to the joystick instance.
 * @param axis JA_X or JA_Y.
 * @param min Raw value at full deflection to the left or down.
 * @param center Raw value at rest. Must lie strictly between min and max.
 * @param max Raw value at full deflection to the right or up.
 * @return `TRUE` if applied, otherwise `FALSE` with `E_INVALID_PARAM`.
 */
BOOL pifJoystick_SetAxis(PifJoystick *p_owner, PifJoystickAxis axis, int16_t min, int16_t center, int16_t max);

/**
 * @brief Turns an axis the other way round, for a module whose Y grows downwards for instance.
 * @param p_owner Pointer to the joystick instance.
 * @param axis JA_X or JA_Y.
 * @param invert TRUE to invert.
 * @return `TRUE` if applied, otherwise `FALSE` with `E_INVALID_PARAM`.
 */
BOOL pifJoystick_SetInvert(PifJoystick *p_owner, PifJoystickAxis axis, BOOL invert);

/**
 * @brief Sets the dead zone. Positions within it read as 0, and the rest is stretched so that full
 *        deflection still reads PIF_JOYSTICK_RANGE.
 * @param p_owner Pointer to the joystick instance.
 * @param dead_zone 0 to PIF_JOYSTICK_RANGE - 1, in units of position.
 * @return `TRUE` if applied, otherwise `FALSE` with `E_INVALID_PARAM`.
 */
BOOL pifJoystick_SetDeadZone(PifJoystick *p_owner, uint16_t dead_zone);

/**
 * @brief Takes one reading as the centre of both axes. Call it while the stick is at rest. Without
 *        act_read, the last reading given with pifJoystick_sigData() is taken.
 * @param p_owner Pointer to the joystick instance.
 * @return `TRUE` if applied. `FALSE` with `E_INVALID_STATE` when no reading has been given yet or the
 *         reading does not lie strictly between the min and max of an axis, in which case neither
 *         axis is changed.
 */
BOOL pifJoystick_CalibrateCenter(PifJoystick *p_owner);

/**
 * @brief Attaches noise filters for the raw X and Y. Nothing is reported until both windows are full.
 * @param p_owner Pointer to the joystick instance.
 * @param p_filter_x Filter applied to raw X values. It takes and gives int16_t (NFV_INT16).
 * @param p_filter_y Filter applied to raw Y values. It takes and gives int16_t (NFV_INT16).
 * @return `TRUE` if both filters are valid and attached, otherwise `FALSE`.
 */
BOOL pifJoystick_AttachFilter(PifJoystick *p_owner, PifNoiseFilter *p_filter_x, PifNoiseFilter *p_filter_y);

/**
 * @brief Gives a reading, through the attached filters if any, for a joystick without act_read. It may
 *        be called from an interrupt, and the position changes at the next pifJoystick_Process().
 *        Of several readings given in between, the last one counts, though every one of them goes
 *        through the filters.
 * @param p_owner Pointer to the joystick instance.
 * @param x Raw X.
 * @param y Raw Y.
 */
void pifJoystick_sigData(PifJoystick *p_owner, int16_t x, int16_t y);

/**
 * @brief Takes a reading from act_read when it is set, then updates the position and the direction
 *        from the latest reading and calls their events. Without act_read it does nothing until
 *        pifJoystick_sigData() has given a new reading.
 *        The task of pifJoystick_Start() calls it; call it from a task of your own instead to share one.
 * @param p_owner Pointer to the joystick instance.
 */
void pifJoystick_Process(PifJoystick *p_owner);

/**
 * @brief Sets the period of the joystick task.
 * @param p_owner Pointer to the joystick instance.
 * @param period1ms New period in milliseconds. Must be greater than zero.
 * @return `TRUE` if the period is valid and applied, otherwise `FALSE`.
 */
BOOL pifJoystick_SetControlPeriod(PifJoystick *p_owner, uint16_t period1ms);

/**
 * @brief Starts or resumes periodic reading.
 * @param p_owner Pointer to the joystick instance.
 * @param p_name Optional task name. If `NULL`, `"Joystick"` is used.
 * @return `TRUE` if reading starts, otherwise `FALSE`.
 */
BOOL pifJoystick_Start(PifJoystick *p_owner, const char *p_name);

/**
 * @brief Pauses periodic reading.
 * @param p_owner Pointer to the joystick instance.
 */
void pifJoystick_Stop(PifJoystick *p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_JOYSTICK_H
