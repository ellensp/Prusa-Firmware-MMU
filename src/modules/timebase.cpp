/// @file timebase.cpp
#include "timebase.h"
#include "../hal/timers.h"
#ifdef __AVR__
#include <avr/interrupt.h>
#endif

namespace modules {
namespace time {

Timebase timebase;

void Timebase::Init() {
#ifdef __STM32__
    hal::timers::InitTick1ms();
#else
    hal::timers::Tim8bit_CTC_config tim8ctc_conf = {
        .cs = 3, // ck/64
        .ocra = 250 - 1,
    };
    hal::timers::Configure_CTC8(0, TIMER0, &tim8ctc_conf);
#endif
}

void Timebase::Isr() {
    ms++;
}

bool Timebase::Elapsed(uint16_t start, uint16_t timeout) const {
    uint16_t ms_from_start = Millis(); // beware the uint16_t!
    ms_from_start -= start;
    return ms_from_start > timeout;
}

uint16_t Timebase::Millis() const {
#ifdef __STM32__
    // ms changes in the SysTick ISR, don't let the compiler hoist it out of wait loops
    return *static_cast<const volatile uint16_t *>(&ms);
#else
    return ms;
#endif
}

} // namespace time
} // namespace modules

#ifdef __STM32__
extern "C" void SysTick_Handler() {
    modules::time::timebase.Isr();
}
#else
ISR(TIMER0_COMPA_vect) {
    modules::time::timebase.Isr();
}
#endif
