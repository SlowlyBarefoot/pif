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
    MS_BREAK        = 7,    // Preparing to engage brake
    MS_BREAKING     = 8,    // Brake engaged
    MS_STOPPING     = 9,    // Stopping transition stage
    MS_STOP         = 10,   // Stopped
    MS_COUNT        = 11
} PifMotorState;

typedef enum EnPifMotorMode
{
    // Direction
    MM_D_MASK           = 0x01,
    MM_D_SHIFT          = 0,
    MM_D_CW             = 0x00,
    MM_D_CCW            = 0x01,

    // Speed control enable
    MM_SC_MASK          = 0x02,
    MM_SC_SHIFT         = 1,
    MM_SC_NO            = 0x00,
    MM_SC_YES           = 0x02,

    // Position control enable
    MM_PC_MASK          = 0x04,
    MM_PC_SHIFT         = 2,
    MM_PC_NO            = 0x00,
    MM_PC_YES           = 0x04,

    // Deceleration trigger mode
    MM_RT_MASK          = 0x18,
    MM_RT_SHIFT         = 3,
    MM_RT_NONE          = 0x00,
    MM_RT_TIME          = 0x08,     // Stop after a fixed operating time

    // Hold output after stop (do not release)
    MM_NR_MASK          = 0x20,
    MM_NR_SHIFT         = 5,
    MM_NR_NO            = 0x00,
    MM_NR_YES           = 0x20,

    // Validate initial sensor states before starting:
    // Start sensor must be ON, Reduce and Stop sensors must be OFF.
    MM_CIAS_MASK        = 0x40,
    MM_CIAS_SHIFT       = 6,
    MM_CIAS_NO          = 0x00,
    MM_CIAS_YES         = 0x40,

    // Verify stop sensor state when stop completes
    MM_CFPS_MASK        = 0x80,
    MM_CFPS_SHIFT       = 7,
    MM_CFPS_NO          = 0x00,
    MM_CFPS_YES         = 0x80,
} PifMotorMode;


extern const char* kMotorState[MS_COUNT];


#endif  // PIF_MOTOR_H
