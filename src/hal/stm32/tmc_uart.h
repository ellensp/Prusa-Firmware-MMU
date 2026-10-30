/// @file tmc_uart.h
/// Single-wire UART to the TMC2209 drivers of the MMU2-DIP.
/// Their PDN_UART pins sit on plain GPIOs, so the UART is bit-banged:
/// TIM17 times the bits, an EXTI interrupt catches the start bit of each received byte.
#pragma once
#include <stdint.h>
#include "../gpio.h"

namespace hal {
namespace tmc_uart {

static constexpr uint32_t baudrate = 115200;

/// Set up the bit timer and the EXTI interrupts
void Init();

/// Send @p txLen bytes on @p pin, then receive @p rxLen bytes (0 = nothing to receive).
/// Blocks until done. Interrupts must be enabled.
/// @returns false if the driver didn't answer in time
bool Transfer(const gpio::GPIO_pin pin, const uint8_t *tx, uint8_t txLen, uint8_t *rx, uint8_t rxLen);

} // namespace tmc_uart
} // namespace hal
