/// @file cpu.cpp
#include "../cpu.h"
#include "../usart.h"
#include "cmsis.h"

namespace hal {
namespace cpu {

bool resetPending = false;

/// Run from the PLL at 48 MHz: the 8 MHz crystal x6, or HSI/2 x12 if the crystal doesn't start
static void InitClock() {
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY; // 1 wait state for 48 MHz

    RCC->CR |= RCC_CR_HSEON;
    bool hse = false;
    for (uint32_t i = 0; i < 100000U; ++i) {
        if (RCC->CR & RCC_CR_HSERDY) {
            hse = true;
            break;
        }
    }

    if (hse) {
        RCC->CFGR2 = RCC_CFGR2_PREDIV_DIV1;
        RCC->CFGR = RCC_CFGR_PLLSRC_HSE_PREDIV | RCC_CFGR_PLLMUL6;
    } else {
        RCC->CR &= ~RCC_CR_HSEON;
        RCC->CFGR = RCC_CFGR_PLLSRC_HSI_DIV2 | RCC_CFGR_PLLMUL12;
    }

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
        ;
}

void Init() {
    InitClock();
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOFEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
}

void Reset() {
    __disable_irq();
    NVIC_SystemReset();
}

void Step() {
    if (resetPending) {
        hal::usart::usart1.puts("resetPending\n");
        for (;;)
            ; //endless loop while waiting for the watchdog to reset
    }
}

} // namespace CPU
} // namespace hal
