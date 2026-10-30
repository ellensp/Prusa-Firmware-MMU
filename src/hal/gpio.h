/// @file gpio.h
#pragma once
#include <inttypes.h>

#ifdef __AVR__
#include <avr/io.h>
#include <util/atomic.h>
#elif defined(__STM32__)
// no CMSIS here - its short macro names (CRC, ADC, ...) would leak into the whole firmware
#else
#define ATOMIC_BLOCK(x)
#endif

namespace hal {

/// GPIO interface
namespace gpio {

#ifndef __STM32__
struct GPIO_TypeDef {
    volatile uint8_t PINx;
    volatile uint8_t DDRx;
    volatile uint8_t PORTx;
};
#endif

enum class Mode : uint8_t {
    input = 0,
    output,
};

enum class Pull : uint8_t {
    none = 0,
    up,
    down, // not available on the AVR
};

enum class Level : uint8_t {
    low = 0,
    high,
};

struct GPIO_InitTypeDef {
    Mode mode;
    Pull pull;
    Level level;
    inline GPIO_InitTypeDef(Mode mode, Pull pull)
        : mode(mode)
        , pull(pull) {};
    inline GPIO_InitTypeDef(Mode mode, Level level)
        : mode(mode)
        , level(level) {};
};

#ifdef __STM32__

/// STM32F0 GPIO registers (checked against CMSIS in stm32/gpio.cpp)
struct GPIO_TypeDef {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
    volatile uint32_t BRR;
};

/// GPIO port, identified by its base address so that pins stay constexpr
enum class Port : uint32_t {
    A = 0x48000000,
    B = 0x48000400,
    C = 0x48000800,
    F = 0x48001400,
};

struct GPIO_pin {
    Port port;
    uint16_t pin_mask;
};

__attribute__((always_inline)) inline GPIO_TypeDef *Regs(const GPIO_pin portPin) {
    return reinterpret_cast<GPIO_TypeDef *>(static_cast<uint32_t>(portPin.port));
}

__attribute__((always_inline)) inline void WritePin(const GPIO_pin portPin, Level level) {
    // BSRR is atomic, no need to disable interrupts
    Regs(portPin)->BSRR = (level == Level::high) ? portPin.pin_mask : (uint32_t)portPin.pin_mask << 16;
}

__attribute__((always_inline)) inline Level ReadPin(const GPIO_pin portPin) {
    return (Level)((Regs(portPin)->IDR & portPin.pin_mask) != 0);
}

__attribute__((always_inline)) inline void TogglePin(const GPIO_pin portPin) {
    // Touch only this pin - the TMC UART ISR drives other pins of the same port
    GPIO_TypeDef *const port = Regs(portPin);
    port->BSRR = (port->ODR & portPin.pin_mask) ? (uint32_t)portPin.pin_mask << 16 : portPin.pin_mask;
}

/// Raw pin configuration
/// @param moder MODER value: 0 = input, 1 = output, 2 = alternate function, 3 = analog
/// @param pupdr PUPDR value: 0 = none, 1 = pull-up, 2 = pull-down
/// @param af alternate function number (only used if moder == 2)
void Configure(const GPIO_pin portPin, uint8_t moder, uint8_t pupdr, uint8_t af = 0);

inline void Init(const GPIO_pin portPin, GPIO_InitTypeDef GPIO_Init) {
    if (GPIO_Init.mode == Mode::output) {
        WritePin(portPin, GPIO_Init.level);
        Configure(portPin, 1, 0);
    } else {
        Configure(portPin, 0, (uint8_t)GPIO_Init.pull);
    }
}

#else

struct GPIO_pin {
    // No constructor here in order to allow brace-initialization in old
    // gcc versions/standards
    GPIO_TypeDef *const port;
    const uint8_t pin_mask;
};

__attribute__((always_inline)) inline void WritePin(const GPIO_pin portPin, Level level) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (level == Level::high)
            portPin.port->PORTx |= portPin.pin_mask;
        else
            portPin.port->PORTx &= ~portPin.pin_mask;
    }
}

__attribute__((always_inline)) inline Level ReadPin(const GPIO_pin portPin) {
#ifdef __AVR__
    return (Level)((portPin.port->PINx & portPin.pin_mask) != 0);
#else
    // Return the value modified by WritePin
    return (Level)((portPin.port->PORTx & portPin.pin_mask) != 0);
#endif
}

__attribute__((always_inline)) inline void TogglePin(const GPIO_pin portPin) {
#ifdef __AVR__
    // Optimized path for AVR, resulting in a pin toggle
    portPin.port->PINx = portPin.pin_mask;
#else
    WritePin(portPin, (Level)(ReadPin(portPin) != Level::high));
#endif
}

__attribute__((always_inline)) inline void Init(const GPIO_pin portPin, GPIO_InitTypeDef GPIO_Init) {
    if (GPIO_Init.mode == Mode::output) {
        WritePin(portPin, GPIO_Init.level);
        portPin.port->DDRx |= portPin.pin_mask;
    } else {
        portPin.port->DDRx &= ~portPin.pin_mask;
        WritePin(portPin, (Level)GPIO_Init.pull);
    }
}

#endif

}
}

#ifdef __STM32__
// pins.h uses hal::gpio::Port
#elif defined(__AVR__)
#define GPIOA ((hal::gpio::GPIO_TypeDef *)&PINA)
#define GPIOB ((hal::gpio::GPIO_TypeDef *)&PINB)
#define GPIOC ((hal::gpio::GPIO_TypeDef *)&PINC)
#define GPIOD ((hal::gpio::GPIO_TypeDef *)&PIND)
#define GPIOE ((hal::gpio::GPIO_TypeDef *)&PINE)
#define GPIOF ((hal::gpio::GPIO_TypeDef *)&PINF)
#define GPIOG ((hal::gpio::GPIO_TypeDef *)&PING)
#define GPIOH ((hal::gpio::GPIO_TypeDef *)&PINH)
#define GPIOJ ((hal::gpio::GPIO_TypeDef *)&PINJ)
#define GPIOK ((hal::gpio::GPIO_TypeDef *)&PINK)
#define GPIOL ((hal::gpio::GPIO_TypeDef *)&PINL)
#else

// stub entries
extern hal::gpio::GPIO_TypeDef _GPIOA;
extern hal::gpio::GPIO_TypeDef _GPIOB;
extern hal::gpio::GPIO_TypeDef _GPIOC;
extern hal::gpio::GPIO_TypeDef _GPIOD;
extern hal::gpio::GPIO_TypeDef _GPIOE;
extern hal::gpio::GPIO_TypeDef _GPIOF;
extern hal::gpio::GPIO_TypeDef _GPIOG;
extern hal::gpio::GPIO_TypeDef _GPIOH;
extern hal::gpio::GPIO_TypeDef _GPIOJ;
extern hal::gpio::GPIO_TypeDef _GPIOK;
extern hal::gpio::GPIO_TypeDef _GPIOL;

#define GPIOA (&::_GPIOA)
#define GPIOB (&::_GPIOB)
#define GPIOC (&::_GPIOC)
#define GPIOD (&::_GPIOD)
#define GPIOE (&::_GPIOE)
#define GPIOF (&::_GPIOF)
#define GPIOG (&::_GPIOG)
#define GPIOH (&::_GPIOH)
#define GPIOJ (&::_GPIOJ)
#define GPIOK (&::_GPIOK)
#define GPIOL (&::_GPIOL)

#endif
