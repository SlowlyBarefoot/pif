// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_motor.h
 * @brief States, stage modes and errors shared by the duty motor and step motor modules.
 *
 * A motor runs a stage at a time. The mode byte of a stage chooses the direction, the speed and position
 * control, how the deceleration is triggered and what is checked at the start and at the stop.
 */
#ifndef PIF_MOTOR_H
#define PIF_MOTOR_H


#include "core/pif.h"


/**
 * @brief Stage of a motor run, from the start to the stop.
 */
typedef enum EnPifMotorState
{
    MS_IDLE         = 0,	///< Not running
    MS_GAINED       = 1,    ///< Acceleration stage
    MS_STABLE       = 2,    ///< Stability check stage before constant speed
    MS_CONST        = 3,    ///< Constant-speed stage
    MS_REDUCE       = 4,    ///< Deceleration stage
    MS_LOW_CONST    = 5,    ///< Low-speed stage for precise final approach
    MS_OVER_RUN     = 6,    ///< Delay stage after stop trigger before deceleration
    MS_BRAKE        = 7,    ///< Preparing to engage brake
    MS_BRAKING     = 8,    ///< Brake engaged
    MS_STOPPING     = 9,    ///< Stopping transition stage
    MS_STOP         = 10,   ///< Stopped
    MS_COUNT        = 11	///< Number of states
} PifMotorState;

/** @name Stage mode fields
 *  Mask and bit position of each PifMotorMode group in the stage mode byte.
 *  @{
 */
#define MM_D_MASK           0x01    ///< Direction
#define MM_D_SHIFT          0		///< Bit position of MM_D_MASK
#define MM_SC_MASK          0x02    ///< Speed control enable
#define MM_SC_SHIFT         1		///< Bit position of MM_SC_MASK
#define MM_PC_MASK          0x04    ///< Position control enable
#define MM_PC_SHIFT         2		///< Bit position of MM_PC_MASK
#define MM_RT_MASK          0x18    ///< Deceleration trigger mode
#define MM_RT_SHIFT         3		///< Bit position of MM_RT_MASK
#define MM_NR_MASK          0x20    ///< Hold output after stop
#define MM_NR_SHIFT         5		///< Bit position of MM_NR_MASK
#define MM_CIAS_MASK        0x40    ///< Validate initial sensor states before starting
#define MM_CIAS_SHIFT       6		///< Bit position of MM_CIAS_MASK
#define MM_CFPS_MASK        0x80    ///< Verify stop sensor state when stop completes
#define MM_CFPS_SHIFT       7		///< Bit position of MM_CFPS_MASK
/** @} */

/// Options of the stage mode byte: OR one value of each group, e.g. MM_D_CCW | MM_SC_YES | MM_RT_TIME.
typedef enum EnPifMotorMode
{
    /// Direction (MM_D_MASK)
    MM_D_CW             = 0x00,
    MM_D_CCW            = 0x01,

    /// Speed control enable (MM_SC_MASK)
    MM_SC_NO            = 0x00,
    MM_SC_YES           = 0x02,

    /// Position control enable (MM_PC_MASK)
    MM_PC_NO            = 0x00,
    MM_PC_YES           = 0x04,

    /// Deceleration trigger mode (MM_RT_MASK, 0x10 and 0x18 are reserved)
    MM_RT_NONE          = 0x00,
    MM_RT_TIME          = 0x08,     ///< Stop after a fixed operating time

    /// Hold output after stop, do not release (MM_NR_MASK)
    MM_NR_NO            = 0x00,
    MM_NR_YES           = 0x20,

    /// Validate initial sensor states before starting (MM_CIAS_MASK):
    /// Start sensor must be ON, Reduce and Stop sensors must be OFF.
    MM_CIAS_NO          = 0x00,
    MM_CIAS_YES         = 0x40,

    /// Verify stop sensor state when stop completes (MM_CFPS_MASK)
    MM_CFPS_NO          = 0x00,
    MM_CFPS_YES         = 0x80
} PifMotorMode;

/**
 * @brief Cause of the last error of a motor.
 */
typedef enum EnPifMotorError
{
    ME_NONE             = 0,	///< No error
    ME_STOP_POSITION    = 1,    ///< Stop sensor was OFF when the stop completed (MM_CFPS_YES)
    ME_TIMER_START      = 2,    ///< A control timer could not be started
    ME_STABLE_TIMEOUT   = 3     ///< The speed did not settle within the stage timeout
} PifMotorError;


#ifdef __cplusplus
extern "C" {
#endif

extern const char* const kMotorState[MS_COUNT];	///< Names of the states, for logs

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_H
