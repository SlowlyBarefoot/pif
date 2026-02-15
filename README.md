# PIF

PIF (Platform-independent framework) is a portable C framework for embedded systems.

It helps you keep business logic and reusable modules independent from MCU/board-specific code.

## TL;DR

- Keep platform code thin.
- Put reusable logic in PIF modules.
- Connect both sides with `sig*`, `act*`, and `evt*` callbacks.
- Drive periodic signals (for example 1 ms tick) and run tasks in your main loop.

## 5-Minute Quick Start

### 1. Add PIF to your build

Include these directories in your project:

- `include/`
- `source/`

### 2. Create your config header

Start from `include/pif_conf_temp.h` and create `pif_conf.h` in your include path.

Enable only the macros you need for your target.

### 3. Implement platform hooks

Provide platform-specific functions used by selected modules, such as:

- high-resolution timer callback
- GPIO access callbacks
- communication transport callbacks (UART/SPI/I2C, etc.)

### 4. Initialize core

Initialize PIF during startup (before entering the main loop):

```c
#include "core/pif.h"

static uint32_t platform_timer1us(void)
{
    // Return current 1 us tick from your platform.
    return 0;
}

void app_init(void)
{
    if (!pif_Init(platform_timer1us)) {
        // Handle initialization failure
        while (1) {}
    }
}
```

### 5. Drive periodic signals and tasks

Feed the framework tick signal and execute module tasks:

- call `pif_sigTimer1ms()` from your 1 ms timer/ISR context (or equivalent timing path)
- run enabled module task handlers from your main loop

## Build and Integration Examples

This repository does not include a single official build script because integration is usually done from your firmware project.

### GCC (manual compile) example

Use this when you want a quick local compile test:

```bash
gcc -std=c11 -Iinclude -c source/core/pif.c -o build/pif.o
```

To compile with more modules, add additional source files from `source/`:

```bash
gcc -std=c11 -Iinclude \
  -c source/core/pif.c \
  -c source/core/pif_task.c \
  -c source/storage/pif_storage.c
```

### CMake integration example

In your firmware project `CMakeLists.txt`, add PIF as a library:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_firmware C)

set(PIF_ROOT ${CMAKE_CURRENT_LIST_DIR}/third_party/pif)

file(GLOB_RECURSE PIF_SOURCES
    ${PIF_ROOT}/source/*.c
)

add_library(pif STATIC ${PIF_SOURCES})
target_include_directories(pif PUBLIC
    ${PIF_ROOT}/include
)

# Your target
add_executable(app src/main.c)
target_link_libraries(app PRIVATE pif)
```

If you want faster build times, replace `GLOB_RECURSE` with an explicit source list for only the modules you use.

## Integration Model

PIF sits between platform code and application code.

1. Platform -> PIF: `sig*` functions (signals/ticks/inputs).
2. PIF -> Platform: callback pointers.
3. Application -> PIF: public API calls.
4. PIF -> Application: `evt*` callbacks.

Callback naming conventions:

- `act*`: PIF requests an action from platform/app code.
- `evt*`: PIF notifies an event.

## Project Layout

- `include/`: public headers grouped by domain
- `source/`: implementation files grouped by domain

Representative domains:

- `core`, `communication`, `protocol`, `sensor`, `input`, `storage`
- `display`, `motor`, `sound`, `gps`, `filter`, `interpreter`
- `markup`, `rc`, `osd`, `actulator`

## Coding Conventions Used in PIF

Since C has no built-in member visibility, PIF uses naming rules:

- `__name`: private/internal field (do not access outside owner module)
- `_name`: externally read-only field (do not modify directly)

## Example Project

Working example:

- https://github.com/SlowlyBarefoot/pif-example

## Upstream References and Credits

Some modules were influenced by these projects.

### Interpreter

- basic: https://github.com/jwillia3/BASIC (Jerry Williams Jr)

### Protocol

- ibus: https://github.com/bmelink/IBusBM
- msp: https://github.com/multiwii/baseflight
- sbus: https://github.com/zendes/SBUS
- sumd: https://github.com/Benoit3/Sumd-for-Arduino
- spektrum: https://github.com/SpektrumRC/SRXL2

### Sensor

- bmp280: https://github.com/MartinL1/BMP280_DEV
- bmp280 reference: https://github.com/multiwii/baseflight

### Sound

- buzzer: https://github.com/multiwii/baseflight

## License

See `LICENSE`.
