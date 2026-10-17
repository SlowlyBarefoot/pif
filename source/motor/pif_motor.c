// SPDX-License-Identifier: BSD-3-Clause
#include "motor/pif_motor.h"


/**
 * @brief Human-readable names for each motor state.
 *
 * Each entry is placed at its PifMotorState index so log messages and
 * debug output can print state names directly.
 */
const char* const kMotorState[MS_COUNT] =
{
    [MS_IDLE]       = "Idle",
    [MS_GAINED]     = "Gained",
    [MS_STABLE]     = "Stable",
    [MS_CONST]      = "Const",
    [MS_REDUCE]     = "Reduce",
    [MS_LOW_CONST]  = "LowConst",
    [MS_OVER_RUN]   = "OverRun",
    [MS_BRAKE]      = "Brake",
    [MS_BRAKING]   = "Braking",
    [MS_STOPPING]   = "Stopping",
    [MS_STOP]       = "Stop"
};

