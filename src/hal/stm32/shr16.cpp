/// @file shr16.cpp
/// The MMU2-DIP drives its 2x74HC595 exactly like the Original Prusa MMU board,
/// only the pins differ - see pins.h
#include "../shr16.h"
#include "../gpio.h"
#include "../../pins.h"
#include "irq.h"

#define SHR16_LED_MSK 0xffc0
#define SHR16_DIR_MSK 0x0015
#define SHR16_ENA_MSK 0x002A

namespace hal {
namespace shr16 {

SHR16 shr16;

/// The latch pulse only needs some ns, give it a bit more
static inline void LatchDelay() {
    for (volatile uint8_t i = 0; i < 8; ++i)
        ;
}

void SHR16::Init() {
    using namespace hal::gpio;
    gpio::Init(SHR16_DATA, GPIO_InitTypeDef(Mode::output, Level::low));
    gpio::Init(SHR16_LATCH, GPIO_InitTypeDef(Mode::output, Level::low));
    gpio::Init(SHR16_CLOCK, GPIO_InitTypeDef(Mode::output, Level::low));
    Write(SHR16_ENA_MSK);
}

/// Called from the main loop and from the step ISR (direction changes), callers must hold a StepIsrGuard
void SHR16::Write(uint16_t v) {
    using namespace hal::gpio;
    for (uint16_t m = 0x8000; m; m >>= 1) {
        WritePin(SHR16_CLOCK, Level::low);
        WritePin(SHR16_DATA, (Level)((m & v) != 0));
        WritePin(SHR16_CLOCK, Level::high);
    }
    WritePin(SHR16_CLOCK, Level::low);
    WritePin(SHR16_LATCH, Level::high);
    LatchDelay();
    WritePin(SHR16_LATCH, Level::low);

    shr16_v = v;
}

void SHR16::SetLED(uint16_t led) {
    irq::StepIsrGuard guard;
    led = ((led & 0x00ff) << 8) | ((led & 0x0300) >> 2);
    Write((shr16_v & ~SHR16_LED_MSK) | led);
}

void SHR16::SetTMCEnabled(uint8_t index, bool ena) {
    irq::StepIsrGuard guard;
    const uint16_t mask = 1 << (2 * index + 1);
    if (ena)
        Write(shr16_v & ~mask);
    else
        Write(shr16_v | mask);
}

void SHR16::SetTMCDir(uint8_t index, bool dir) {
    irq::StepIsrGuard guard;
    const uint16_t mask = 1 << (2 * index);
    if (dir)
        Write(shr16_v & ~mask);
    else
        Write(shr16_v | mask);
}

} // namespace shr16
} // namespace hal
