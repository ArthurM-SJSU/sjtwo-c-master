#pragma once

#include <stdint.h>

void ssp0_init(uint32_t max_clock_khz);

void ssp0__set_max_clock(uint32_t max_clock_khz);

uint8_t ssp0_byte_exchange(uint8_t byte);

uint16_t ssp0_SCI_READ(uint8_t address);