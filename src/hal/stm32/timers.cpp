/// @file timers.cpp
#include "../timers.h"
#include "cmsis.h"
#include "irq.h"

namespace hal {
namespace timers {

void InitTick1ms() {
    SysTick->LOAD = F_CPU / 1000 - 1;
    SysTick->VAL = 0;
    NVIC_SetPriority(SysTick_IRQn, irq::prioTick);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

void InitStepTimer(uint16_t divider, uint16_t interval) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;

    TIM3->CR1 = 0; // ARR not preloaded - a new interval applies to the running period
    TIM3->DIER = 0;
    TIM3->PSC = divider - 1;
    TIM3->ARR = interval;
    TIM3->EGR = TIM_EGR_UG; // load the prescaler
    TIM3->SR = 0;
    TIM3->CNT = 0;

    NVIC_SetPriority(TIM3_IRQn, irq::prioStep);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1 = TIM_CR1_CEN;
}

void StepIsrSetEnabled(bool state) {
    irq::CriticalSection cs;
    if (state)
        TIM3->DIER |= TIM_DIER_UIE;
    else
        TIM3->DIER &= ~TIM_DIER_UIE;
}

bool StepIsrDisable() {
    irq::CriticalSection cs;
    const bool state = TIM3->DIER & TIM_DIER_UIE;
    TIM3->DIER &= ~TIM_DIER_UIE;
    return state;
}

void StepIsrAck() {
    TIM3->SR = ~TIM_SR_UIF;
}

void SetStepInterval(uint16_t interval) {
    TIM3->ARR = interval;
    // if the handler took longer than the new interval, don't wait for a full counter wrap
    if (TIM3->CNT >= interval)
        TIM3->EGR = TIM_EGR_UG;
}

} // namespace timers
} // namespace hal
