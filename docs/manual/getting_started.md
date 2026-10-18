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
macros your target needs.

## Initialize the core

Call pif_Init() once during startup, before the main loop, with a function that returns a 1 us tick.

@code{.c}
#include "core/pif.h"

static uint32_t platform_timer1us(void)
{
    // Return the current 1 us tick of your platform.
    return 0;
}

void app_init(void)
{
    if (!pif_Init(platform_timer1us)) {
        while (1) {}
    }
}
@endcode

## Drive the tick and the tasks

- Call pif_sigTimer1ms() every 1 ms, from a timer interrupt for example.
- Call pifTaskManager_Loop() from the main loop.
