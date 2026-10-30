/// @file gpio.cpp
#include "../gpio.h"
#include "irq.h"
#include <stddef.h>

namespace hal {
namespace gpio {

static_assert(sizeof(GPIO_TypeDef) == sizeof(::GPIO_TypeDef));
static_assert(offsetof(GPIO_TypeDef, AFR) == offsetof(::GPIO_TypeDef, AFR));
static_assert(offsetof(GPIO_TypeDef, BRR) == offsetof(::GPIO_TypeDef, BRR));
static_assert(static_cast<uint32_t>(Port::A) == GPIOA_BASE && static_cast<uint32_t>(Port::B) == GPIOB_BASE);
static_assert(static_cast<uint32_t>(Port::C) == GPIOC_BASE && static_cast<uint32_t>(Port::F) == GPIOF_BASE);

void Configure(const GPIO_pin portPin, uint8_t moder, uint8_t pupdr, uint8_t af) {
    GPIO_TypeDef *const port = Regs(portPin);
    const uint32_t pin = __builtin_ctz(portPin.pin_mask);
    const uint32_t shift2 = pin * 2;

    // the TMC UART ISR reconfigures pins of port B, keep it out while we read-modify-write
    irq::CriticalSection cs;
    if (moder == 2) {
        volatile uint32_t &afr = port->AFR[pin >> 3];
        const uint32_t shift4 = (pin & 7) * 4;
        afr = (afr & ~(0xFU << shift4)) | ((uint32_t)af << shift4);
    }
    port->OTYPER &= ~(uint32_t)portPin.pin_mask; // push-pull
    port->OSPEEDR |= 3U << shift2; // high speed
    port->PUPDR = (port->PUPDR & ~(3U << shift2)) | ((uint32_t)pupdr << shift2);
    port->MODER = (port->MODER & ~(3U << shift2)) | ((uint32_t)moder << shift2);
}

} // namespace gpio
} // namespace hal
