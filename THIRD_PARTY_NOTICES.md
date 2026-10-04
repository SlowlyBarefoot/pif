# Third-Party Notices

PIF is licensed under BSD-3-Clause, except for the `gpl/` directory, which is
licensed under GPL-3.0-or-later. Some files contain or are derived from code
written by others. This file lists that code, its origin and its license.
Full license texts are in `LICENSES/`.

## GPL-3.0-or-later code (`gpl/`)

The following modules contain code derived from GPL-3.0 flight controller
projects. They live under `gpl/` and are distributed under GPL-3.0-or-later.

| File | Derived from |
| --- | --- |
| `gpl/source/gps/pif_gps.c` | Betaflight / Cleanflight `gps.c`, `gps_conversion.c`; Baseflight `gps.c` (MultiWii lineage) |
| `gpl/include/gps/pif_gps.h` | Baseflight `mw.c` (GPS state fields) |
| `gpl/source/gps/pif_gps_ublox.c`, `gpl/include/gps/pif_gps_ublox.h` | Betaflight / Cleanflight / Baseflight `gps.c` (u-blox NAV decoding); Baseflight `mw.c` |
| `gpl/source/motor/pif_dshot.c` | Betaflight `dshot.c`, `dshot_command.c`, `dshot_dpwm.c`, `dshot_bitbang_decode.c` |
| `gpl/source/osd/pif_max7456.c` | Betaflight `max7456.c` |
| `gpl/source/protocol/pif_msp_v2.c` | Betaflight `msp_serial.c` |
| `gpl/source/sensor/pif_hmc5883.c` | Baseflight `drv_hmc5883l.c` (also in Cleanflight `compass_hmc5883l.c`; MultiWii lineage) |
| `gpl/source/sensor/pif_ms5611.c` | Baseflight `drv_ms5611.c` (PROM CRC check and compensation) |
| `gpl/source/sensor/pif_gy86.c` | Baseflight `drv_mpu.c` (MPU setup comments) |

`pif_gps_nmea` is also placed under `gpl/` because it depends on `pif_gps`, and
`pif_gy86` because it depends on `pif_hmc5883` and `pif_ms5611`.

Upstream projects and copyright holders:

- Betaflight - https://github.com/betaflight/betaflight
  Copyright (C) Betaflight and Cleanflight contributors. GPL-3.0-or-later.
- Cleanflight - https://github.com/cleanflight/cleanflight
  Copyright (C) Cleanflight contributors. GPL-3.0-or-later.
- Baseflight - https://github.com/multiwii/baseflight
  Copyright (C) Baseflight contributors. GPL-3.0.
- MultiWii - https://github.com/multiwii/multiwii-firmware
  Copyright (C) MultiWii contributors. GPL-3.0.

## Permissive third-party code (`include/`, `source/`)

### BMI270 configuration file

- File: `source/sensor/pif_bmi270.c` (`maximum_fifo_config_file`)
- Origin: Bosch Sensortec BMI270 Sensor API,
  https://github.com/boschsensortec/BMI270_SensorAPI
- License: BSD-3-Clause

```
Copyright (c) 2020 Bosch Sensortec GmbH. All rights reserved.

BSD-3-Clause

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

### DPS310 driver

- Files: `include/sensor/pif_dps310.h`, `source/sensor/pif_dps310.c`
- Origin: Infineon DPS310 Pressure Sensor Arduino library,
  https://github.com/Infineon/DPS310-Pressure-Sensor
- License: MIT (`LICENSES/MIT.txt`)
- Copyright (c) 2017 Infineon Technologies AG

### BASIC interpreter

- Files: `include/interpreter/pif_basic.h`, `source/interpreter/pif_basic.c`
- Origin: https://github.com/jwillia3/BASIC
- License: MIT (`LICENSES/MIT.txt`)
- Copyright 2011 Jerry Williams Jr
