// SPDX-License-Identifier: BSD-3-Clause
// Stands in for tusb.h in the host tests: the CDC calls pif_usb_cdc.c makes, which the test implements.
#ifndef TUSB_H
#define TUSB_H

#include <stdbool.h>
#include <stdint.h>

uint32_t tud_cdc_n_write_available(uint8_t itf);
uint32_t tud_cdc_n_write(uint8_t itf, const void *buffer, uint32_t bufsize);
uint32_t tud_cdc_n_write_flush(uint8_t itf);
uint32_t tud_cdc_n_available(uint8_t itf);
uint32_t tud_cdc_n_read(uint8_t itf, void *buffer, uint32_t bufsize);
bool tud_cdc_n_connected(uint8_t itf);

#endif
