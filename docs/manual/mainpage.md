PIF Manual {#mainpage}
==========

PIF (Platform-independent framework) is a portable C framework for embedded systems.
It keeps business logic and reusable modules independent from MCU and board-specific code.

## Contents

- @subpage getting_started
- @subpage integration_model
- @subpage basic_language
- @subpage srml_syntax

## API reference

The reference is generated from the comments in the public headers.

- **Files**: every header, by its include string (for example `core/pif.h`).
- **Data Structures**: the objects (`PifTask`, `PifRingBuffer`, ...) and their members.

Headers under `gpl/include` are licensed under GPL-3.0-or-later. Building any of them makes the
firmware subject to the GPL-3.0. Everything else is BSD-3-Clause.

## Example project

Working examples for STM32 Nucleo, Arduino and Linux boards are in
[pif-example](https://github.com/SlowlyBarefoot/pif-example).
