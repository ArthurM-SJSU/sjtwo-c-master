#include "boot_leg_spi.h"
#include "clock.h"
#include "lpc40xx.h"
#include "lpc_peripherals.h"
#include <stdbool.h>
#include <stddef.h>

void ssp0_init(uint32_t max_clock_khz) {
  lpc_peripheral__turn_on_power_to(LPC_PERIPHERAL__SSP0);

  LPC_SSP0->CR0 = 7;        // 8-bit mode
  LPC_SSP0->CR1 = (1 << 1); // Enable SSP as Master

  ssp0__set_max_clock(max_clock_khz);

  // init SCK
  LPC_IOCON->P0_15 &= ~(7 << 0);
  LPC_IOCON->P0_15 |= (2 << 0);

  // init MISO
  LPC_IOCON->P0_17 &= ~(7 << 0);
  LPC_IOCON->P0_17 |= (2 << 0);

  // init MOSI
  LPC_IOCON->P0_18 &= ~(7 << 0);
  LPC_IOCON->P0_18 |= (2 << 0);
}

void ssp0__set_max_clock(uint32_t max_clock_khz) {
  uint8_t divider = 2;
  const uint32_t cpu_clock_khz = clock__get_core_clock_hz() / 1000UL;

  // Keep scaling down divider until calculated is higher
  while (max_clock_khz < (cpu_clock_khz / divider) && divider <= 254) {
    divider += 2;
  }

  LPC_SSP0->CPSR = divider;
}

uint8_t ssp0_byte_exchange(uint8_t byte) {
  LPC_SSP0->DR = byte;

  while (LPC_SSP0->SR & (1 << 4)) {
    ; // Wait until SSP is busy
  }

  return (uint8_t)(LPC_SSP0->DR & 0xFF);
}

uint16_t ssp0_SCI_READ(uint8_t address) {
  LPC_SSP0->DR = address;

  while (LPC_SSP0->SR & (1 << 4)) {
    ; // Wait until SSP is busy
  }

  return (uint16_t)(LPC_SSP0->DR & 0xFFFF);
}