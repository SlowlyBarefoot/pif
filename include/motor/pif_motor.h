// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MOTOR_H
#define PIF_MOTOR_H


#include "core/pif.h"


typedef enum EnPifMotorState
{
    MS_IDLE         = 0,
    MS_GAINED       = 1,    // Acceleration stage
    MS_STABLE       = 2,    // Stability check stage before constant speed
    MS_CONST        = 3,    // Constant-speed stage
    MS_REDUCE       = 4,    // Deceleration stage
    MS_LOW_CONST    = 5,    // Low-speed stage for precise final approach
    MS_OVER_RUN     = 6,    // Delay stage after stop trigger before deceleration
    MS_BRAKE        = 7,    // Preparing to engage brake
    MS_BRAKING     = 8,    // Brake engaged
    MS_STOPPING     = 9,    // Stopping transition stage
    MS_STOP         = 10,   // Stopped
    MS_COUNT        = 11
} PifMotorState;

// Fields of the stage mode byte: the mask and the bit position of each PifMotorMode group.
#define MM_D_MASK           0x01    // Direction
#define MM_D_SHIFT          0
#define MM_SC_MASK          0x02    // Speed control enable
#define MM_SC_SHIFT         1
#define MM_PC_MASK          0x04    // Position control enable
#define MM_PC_SHIFT         2
#define MM_RT_MASK          0x18    // Deceleration trigger mode
#define MM_RT_SHIFT         3
#define MM_NR_MASK          0x20    // Hold output after stop
#define MM_NR_SHIFT         5
#define MM_CIAS_MASK        0x40    // Validate initial sensor states before starting
#define MM_CIAS_SHIFT       6
#define MM_CFPS_MASK        0x80    // Verify stop sensor state when stop completes
#define MM_CFPS_SHIFT       7

// Options of the stage mode byte: OR one value of each group, e.g. MM_D_CCW | MM_SC_YES | MM_RT_TIME.
typedef enum EnPifMotorMode
{
    // Direction (MM_D_MASK)
    MM_D_CW             = 0x00,
    MM_D_CCW            = 0x01,

    // Speed control enable (MM_SC_MASK)
    MM_SC_NO            = 0x00,
    MM_SC_YES           = 0x02,

    // Position control enable (MM_PC_MASK)
    MM_PC_NO            = 0x00,
    MM_PC_YES           = 0x04,

    // Deceleration trigger mode (MM_RT_MASK, 0x10 and 0x18 are reserved)
    MM_RT_NONE          = 0x00,
    MM_RT_TIME          = 0x08,     // Stop after a fixed operating time

    // Hold output after stop, do not release (MM_NR_MASK)
    MM_NR_NO            = 0x00,
    MM_NR_YES           = 0x20,

    // Validate initial sensor states before starting (MM_CIAS_MASK):
    // Start sensor must be ON, Reduce and Stop sensors must be OFF.
    MM_CIAS_NO          = 0x00,
    MM_CIAS_YES         = 0x40,

    // Verify stop sensor state when stop completes (MM_CFPS_MASK)
    MM_CFPS_NO          = 0x00,
    MM_CFPS_YES         = 0x80
} PifMotorMode;

typedef enum EnPifMotorError
{
    ME_NONE             = 0,
    ME_STOP_POSITION    = 1,    // Stop sensor was OFF when the stop completed (MM_CFPS_YES)
    ME_TIMER_START      = 2,    // A control timer could not be started
    ME_STABLE_TIMEOUT   = 3     // The speed did not settle within the stage timeout
} PifMotorError;


#ifdef __cplusplus
extern "C" {
#endif

extern const char* const kMotorState[MS_COUNT];

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_H
