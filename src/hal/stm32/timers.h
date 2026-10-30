/// @file timers.h
/// STM32 timers: SysTick for the 1 ms timebase, TIM3 for stepping
#pragma once
#include <stdint.h>

namespace hal {

/// Hardware Abstraction Layer for the CPU's internal timers
namespace timers {

/// Start SysTick with a 1 ms period. Its handler lives in modules/timebase.cpp
void InitTick1ms();

/// Start the step timer (TIM3) counting at F_CPU / divider.
/// The interrupt fires after @p interval + 1 ticks, like the AVR's CTC mode.
/// The interrupt itself stays disabled until StepIsrSetEnabled(true).
void InitStepTimer(uint16_t divider, uint16_t interval);

/// Enable or disable the step timer interrupt
void StepIsrSetEnabled(bool state);

/// Disable the step timer interrupt
/// @returns true if it was enabled before
bool StepIsrDisable();

/// Acknowledge the step timer interrupt, call first thing in the handler
void StepIsrAck();

/// Plan the next step interrupt @p interval + 1 ticks after the last one
void SetStepInterval(uint16_t interval);

} // namespace timers
} // namespace hal
