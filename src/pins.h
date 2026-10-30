/// @file pins.h
#pragma once
#include "hal/gpio.h"

#ifdef MMU_BOARD_MMU2_DIP

/// BIGTREETECH MMU2-DIP (STM32F030C8T6) pin definitions
using hal::gpio::Port;

/// SPI1 - only used with TMC2130 drivers
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_MISO_PIN = { Port::A, (1 << 6) };
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_MOSI_PIN = { Port::A, (1 << 7) };
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_SCK_PIN = { Port::A, (1 << 5) };
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_SS_PIN = { Port::A, (1 << 4) }; /// not connected, the STM32 SPI master doesn't need it

static constexpr hal::gpio::GPIO_pin SHR16_DATA = { Port::B, (1 << 15) }; ///DS
static constexpr hal::gpio::GPIO_pin SHR16_LATCH = { Port::B, (1 << 12) }; ///STCP
static constexpr hal::gpio::GPIO_pin SHR16_CLOCK = { Port::B, (1 << 13) }; ///SHCP

/// USART1 - shared by the printer connector and the on-board CH340
static constexpr hal::gpio::GPIO_pin USART_RX = { Port::A, (1 << 10) };
static constexpr hal::gpio::GPIO_pin USART_TX = { Port::A, (1 << 9) };

/// *_CS_PIN is the SPI chip select of a TMC2130, or the single-wire UART of a TMC2209
static constexpr hal::gpio::GPIO_pin PULLEY_CS_PIN = { Port::B, (1 << 1) };
static constexpr hal::gpio::GPIO_pin PULLEY_SG_PIN = { Port::B, (1 << 7) };
static constexpr hal::gpio::GPIO_pin PULLEY_STEP_PIN = { Port::B, (1 << 4) };

static constexpr hal::gpio::GPIO_pin SELECTOR_CS_PIN = { Port::B, (1 << 2) };
static constexpr hal::gpio::GPIO_pin SELECTOR_SG_PIN = { Port::B, (1 << 6) };
static constexpr hal::gpio::GPIO_pin SELECTOR_STEP_PIN = { Port::A, (1 << 11) };

static constexpr hal::gpio::GPIO_pin IDLER_CS_PIN = { Port::B, (1 << 10) };
static constexpr hal::gpio::GPIO_pin IDLER_SG_PIN = { Port::B, (1 << 5) };
static constexpr hal::gpio::GPIO_pin IDLER_STEP_PIN = { Port::A, (1 << 8) };

static constexpr hal::gpio::GPIO_pin FINDA_PIN = { Port::A, (1 << 1) }; /// PA1, "sensor" connector P4

static constexpr hal::gpio::GPIO_pin BUTTONS_PIN = { Port::A, (1 << 2) }; /// PA2 = ADC_IN2, 3 buttons on a resistor ladder

#else

/// pin definitions
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_MISO_PIN = { GPIOB, (1 << 3) };
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_MOSI_PIN = { GPIOB, (1 << 2) };
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_SCK_PIN = { GPIOB, (1 << 1) };
static constexpr hal::gpio::GPIO_pin TMC2130_SPI_SS_PIN = { GPIOB, (1 << 0) };

static constexpr hal::gpio::GPIO_pin SHR16_DATA = { GPIOB, (1 << 5) }; ///DS
static constexpr hal::gpio::GPIO_pin SHR16_LATCH = { GPIOB, (1 << 6) }; ///STCP
static constexpr hal::gpio::GPIO_pin SHR16_CLOCK = { GPIOC, (1 << 7) }; ///SHCP

static constexpr hal::gpio::GPIO_pin USART_RX = { GPIOD, (1 << 2) };
static constexpr hal::gpio::GPIO_pin USART_TX = { GPIOD, (1 << 3) };

static constexpr hal::gpio::GPIO_pin PULLEY_CS_PIN = { GPIOC, (1 << 6) };
static constexpr hal::gpio::GPIO_pin PULLEY_SG_PIN = { GPIOF, (1 << 4) };
static constexpr hal::gpio::GPIO_pin PULLEY_STEP_PIN = { GPIOB, (1 << 4) };

static constexpr hal::gpio::GPIO_pin SELECTOR_CS_PIN = { GPIOD, (1 << 7) };
static constexpr hal::gpio::GPIO_pin SELECTOR_SG_PIN = { GPIOF, (1 << 1) };
static constexpr hal::gpio::GPIO_pin SELECTOR_STEP_PIN = { GPIOD, (1 << 4) };

static constexpr hal::gpio::GPIO_pin IDLER_CS_PIN = { GPIOB, (1 << 7) };
static constexpr hal::gpio::GPIO_pin IDLER_SG_PIN = { GPIOF, (1 << 0) };
static constexpr hal::gpio::GPIO_pin IDLER_STEP_PIN = { GPIOD, (1 << 6) };

static constexpr hal::gpio::GPIO_pin FINDA_PIN = { GPIOF, (1 << 6) }; /// PF6     A1      ADC6/TDI

#endif
