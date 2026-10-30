/// @file spi.cpp
#include "../spi.h"
#include "cmsis.h"

namespace hal {
namespace spi {

static_assert(SPI1_BASE == 0x40013000U, "SPI0 in spi.h");

void Init(SPI_TypeDef *const hal_spi, SPI_InitTypeDef *const conf) {
    ::SPI_TypeDef *const hspi = reinterpret_cast<::SPI_TypeDef *>(hal_spi);
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    // PA5/PA6/PA7 = SPI1 SCK/MISO/MOSI, AF0. The SS pin is not used, chip selects are plain GPIOs.
    gpio::Configure(conf->sck_pin, 2, 0, 0);
    gpio::Configure(conf->miso_pin, 2, 1, 0);
    gpio::Configure(conf->mosi_pin, 2, 0, 0);

    // The AVR prescaler 2 gives the TMC2130 a 4 MHz clock, F_CPU / 16 = 3 MHz is the closest here
    hspi->CR1 = 0;
    hspi->CR2 = (7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH; // 8-bit frames
    hspi->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (3U << SPI_CR1_BR_Pos)
        | ((conf->cpol & 0x01) ? SPI_CR1_CPOL : 0) | ((conf->cpha & 0x01) ? SPI_CR1_CPHA : 0)
        | SPI_CR1_SPE;
}

uint8_t TxRx(SPI_TypeDef *hal_spi, uint8_t val) {
    ::SPI_TypeDef *const hspi = reinterpret_cast<::SPI_TypeDef *>(hal_spi);
    while (!(hspi->SR & SPI_SR_TXE))
        ;
    *reinterpret_cast<volatile uint8_t *>(&hspi->DR) = val;
    while (!(hspi->SR & SPI_SR_RXNE))
        ;
    return *reinterpret_cast<volatile uint8_t *>(&hspi->DR);
}

} // namespace spi
} // namespace hal
