Getting Started {#getting_started}
===============

@tableofcontents

## Add PIF to your build

Add these directories to the include path, and the `.c` files under `source/` to the build:

- `include/`
- `source/`

The GPL-3.0 modules (GPS, DShot, MAX7456, MSPv2, HMC5883, MS5611, GY-86) are kept apart in
`gpl/include/` and `gpl/source/`. Add them only when you accept the GPL-3.0 for your firmware.

## Create the configuration header

Copy `include/pif_conf_temp.h` to `pif_conf.h` somewhere on your include path, and enable only the
macros your target needs. Each module is built with its least features, and each `PIF_USE_` macro puts
one more in, so a feature the target does not use costs neither code nor RAM.

## Initialize the core

Call pif_Init() once during startup, before the main loop, with a function that returns a 1 us tick and the
setups of the port and of the application. The port setup is required and runs first; the application setup
may be NULL. When a setup returns FALSE, pif_Init() ends PIF again with pif_Exit() and returns FALSE.

@code{.c}
#include "core/pif_task_manager.h"

static uint32_t platform_timer1us(void)
{
    // Return the current 1 us tick of your platform.
    return 0;
}

static BOOL port_setup(void)
{
    // The task manager and the drivers of the platform.
    return pifTaskManager_Init(8, 0);
}

static BOOL app_setup(void)
{
    // The tasks of the application.
    return TRUE;
}

void app_init(void)
{
    if (!pif_Init(platform_timer1us, port_setup, app_setup)) {
        while (1) {}
    }
}
@endcode

## Drive the tick and the tasks

- Call pif_sigTimer1ms() every 1 ms, from a timer interrupt for example.
- Call pifTaskManager_Loop() from the main loop.

pif_Init() moves pif_state through two stages. From PST_TIMER, set before the setups run, pif_sigTimer1ms()
advances the time base, so a setup can wait with pif_Delay1ms(). From PST_RUNNING, set once both setups
succeeded, pifTaskManager_Loop() runs the tasks. The timer interrupt and the main loop may therefore start
before the setup is finished, with no flag of their own.
