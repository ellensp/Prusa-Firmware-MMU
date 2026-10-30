/// @file startup.cpp
/// Reset handler and vector table of the STM32F030x8
#include <stdint.h>
#include "cmsis.h"

extern "C" {

// provided by the linker script
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

extern void __libc_init_array();
extern int main();

/// Called by __libc_init_array(), normally provided by crti.o which -nostartfiles leaves out
void _init() {
}

void Reset_Handler();

/// Any unexpected interrupt ends up here, the watchdog will reset the MCU
void Default_Handler() {
    for (;;)
        ;
}

#define WEAK_HANDLER(name) void name() __attribute__((weak, alias("Default_Handler")))

WEAK_HANDLER(NMI_Handler);
WEAK_HANDLER(HardFault_Handler);
WEAK_HANDLER(SVC_Handler);
WEAK_HANDLER(PendSV_Handler);
WEAK_HANDLER(SysTick_Handler);
WEAK_HANDLER(WWDG_IRQHandler);
WEAK_HANDLER(RTC_IRQHandler);
WEAK_HANDLER(FLASH_IRQHandler);
WEAK_HANDLER(RCC_IRQHandler);
WEAK_HANDLER(EXTI0_1_IRQHandler);
WEAK_HANDLER(EXTI2_3_IRQHandler);
WEAK_HANDLER(EXTI4_15_IRQHandler);
WEAK_HANDLER(DMA1_Channel1_IRQHandler);
WEAK_HANDLER(DMA1_Channel2_3_IRQHandler);
WEAK_HANDLER(DMA1_Channel4_5_IRQHandler);
WEAK_HANDLER(ADC1_IRQHandler);
WEAK_HANDLER(TIM1_BRK_UP_TRG_COM_IRQHandler);
WEAK_HANDLER(TIM1_CC_IRQHandler);
WEAK_HANDLER(TIM3_IRQHandler);
WEAK_HANDLER(TIM6_IRQHandler);
WEAK_HANDLER(TIM14_IRQHandler);
WEAK_HANDLER(TIM15_IRQHandler);
WEAK_HANDLER(TIM16_IRQHandler);
WEAK_HANDLER(TIM17_IRQHandler);
WEAK_HANDLER(I2C1_IRQHandler);
WEAK_HANDLER(I2C2_IRQHandler);
WEAK_HANDLER(SPI1_IRQHandler);
WEAK_HANDLER(SPI2_IRQHandler);
WEAK_HANDLER(USART1_IRQHandler);
WEAK_HANDLER(USART2_IRQHandler);

using Vector = void (*)();

__attribute__((section(".isr_vector"), used)) const Vector vectorTable[48] = {
    reinterpret_cast<Vector>(&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    SVC_Handler,
    nullptr,
    nullptr,
    PendSV_Handler,
    SysTick_Handler,
    WWDG_IRQHandler,
    nullptr,
    RTC_IRQHandler,
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_1_IRQHandler,
    EXTI2_3_IRQHandler,
    EXTI4_15_IRQHandler,
    nullptr,
    DMA1_Channel1_IRQHandler,
    DMA1_Channel2_3_IRQHandler,
    DMA1_Channel4_5_IRQHandler,
    ADC1_IRQHandler,
    TIM1_BRK_UP_TRG_COM_IRQHandler,
    TIM1_CC_IRQHandler,
    nullptr,
    TIM3_IRQHandler,
    TIM6_IRQHandler,
    nullptr,
    TIM14_IRQHandler,
    TIM15_IRQHandler,
    TIM16_IRQHandler,
    TIM17_IRQHandler,
    I2C1_IRQHandler,
    I2C2_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    nullptr,
    nullptr,
    nullptr,
};

void Reset_Handler() {
    // Like the AVR, start with interrupts disabled - main() enables them after setup()
    __disable_irq();

    for (uint32_t *src = &_sidata, *dst = &_sdata; dst < &_edata;)
        *dst++ = *src++;
    for (uint32_t *dst = &_sbss; dst < &_ebss;)
        *dst++ = 0;

    __libc_init_array();
    main();
    for (;;)
        ;
}

} // extern "C"
