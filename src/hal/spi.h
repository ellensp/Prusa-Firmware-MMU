/// @file spi.h
#pragma once
#include <inttypes.h>
#include "gpio.h"

#ifdef __AVR__
#define SPI0 ((hal::spi::SPI_TypeDef *const) & SPCR)
#elif defined(__STM32__)
#define SPI0 ((hal::spi::SPI_TypeDef *)0x40013000U) // SPI1, defined here without CMSIS
#endif

namespace hal {

/// SPI interface
namespace spi {

#ifdef __STM32__
struct SPI_TypeDef; // registers are only accessed in stm32/spi.cpp
#else
struct SPI_TypeDef {
    volatile uint8_t SPCRx;
    volatile uint8_t SPSRx;
    volatile uint8_t SPDRx;
};
#endif

struct SPI_InitTypeDef {
    hal::gpio::GPIO_pin miso_pin;
    hal::gpio::GPIO_pin mosi_pin;
    hal::gpio::GPIO_pin sck_pin;
    hal::gpio::GPIO_pin ss_pin;
    uint8_t prescaler;
    uint8_t cpha;
    uint8_t cpol;
};

void Init(SPI_TypeDef *const hspi, SPI_InitTypeDef *const conf);

uint8_t TxRx(SPI_TypeDef *hspi, uint8_t val);

#if defined(__AVR__) || defined(__STM32__)
static SPI_TypeDef *const TmcSpiBus = SPI0;
#else
static SPI_TypeDef *const TmcSpiBus = nullptr;
#endif

}
}
