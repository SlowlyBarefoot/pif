#include "motor/pif_motor.h"


/**
 * @brief Human-readable names for each motor state.
 *
 * The index order matches the PifMotorState enumeration so log messages and
 * debug output can print state names directly.
 */
const char* kMotorState[MS_COUNT] =
{
	    "Idle",
	    "Gained",
	    "Stable",
	    "Const",
	    "Reduce",
	    "LowConst",
	    "OverRun",
	    "Break",
	    "Breaking",
	    "Stopping",
	    "Stop"
};

