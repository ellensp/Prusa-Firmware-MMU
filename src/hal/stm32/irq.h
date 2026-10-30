/// @file irq.h
/// Interrupt priorities and critical sections for the STM32
#pragma once
#include "cmsis.h"

namespace hal {
namespace irq {

/// Cortex-M0 priorities, 0 is the most urgent.
/// The TMC2209 UART is bit-banged from interrupts and must not be delayed,
/// the printer USART has a single byte of buffering.
static constexpr uint32_t prioTmcUart = 0;
static constexpr uint32_t prioUsart = 1;
static constexpr uint32_t prioStep = 2;
static constexpr uint32_t prioTick = 3;

/// Disables interrupts for its lifetime, restoring the previous state on exit
class CriticalSection {
public:
    inline CriticalSection()
        : primask(__get_PRIMASK()) {
        __disable_irq();
    }
    inline ~CriticalSection() {
        __set_PRIMASK(primask);
    }

private:
    uint32_t primask;
};

/// Masks only the step timer interrupt for its lifetime.
/// For data shared by the main loop and the step ISR - unlike CriticalSection
/// it doesn't delay the TMC UART bit timing.
class StepIsrGuard {
public:
    inline StepIsrGuard()
        : enabled(NVIC_GetEnableIRQ(TIM3_IRQn)) {
        NVIC_DisableIRQ(TIM3_IRQn);
        __DSB();
        __ISB();
    }
    inline ~StepIsrGuard() {
        if (enabled)
            NVIC_EnableIRQ(TIM3_IRQn);
    }

private:
    uint32_t enabled;
};

} // namespace irq
} // namespace hal
