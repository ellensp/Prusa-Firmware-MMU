/// @file usart.h
/// STM32 implementation of the hal::usart interface, see hal/usart.h
#pragma once
#include <inttypes.h>
#include "../gpio.h"
#include "../circular_buffer.h"

namespace hal {

namespace usart {

class USART {
public:
    struct USART_TypeDef; // registers are only accessed in usart.cpp

    struct USART_InitTypeDef {
        hal::gpio::GPIO_pin rx_pin;
        hal::gpio::GPIO_pin tx_pin;
        uint32_t baudrate;
    };

    /// @returns current character from the UART without extracting it from the read buffer
    uint8_t Peek() const {
        Barrier();
        return rx_buf.front();
    }
    /// @returns true if there are no bytes to be read
    bool ReadEmpty() const {
        Barrier();
        return rx_buf.empty();
    }
    /// @returns current character from the UART and extracts it from the read buffer
    uint8_t Read();

    /// @param c character to be pushed into the TX buffer (to be sent)
    void Write(uint8_t c);
    /// @param str pointer to a string in RAM to be pushed byte-by-byte into the TX buffer (to be sent)
    /// No NL character is appended
    void WriteS(const char *str);
    /// Same as WriteS, there is no separate program memory space on the STM32
    void WriteS_P(const char *str_P) { WriteS(str_P); }

    /// @param str c string to be sent. NL is appended
    void puts(const char *str);
    /// Same as puts
    void puts_P(const char *str_P) { puts(str_P); }
    /// @returns true if there is at least one byte free in the TX buffer (i.e. some space to add a character to be sent)
    bool CanWrite() const {
        Barrier();
        return !tx_buf.full();
    }
    /// blocks until the TX buffer was successfully transmitted
    void Flush();

    /// Enable the RX receiver
    void rx_enable();

    /// Initializes USART interface
    void Init(USART_InitTypeDef *const conf);

    /// implementation of the USART interrupt handler's body
    void ISR();

    USART(uint32_t base)
        : husart(reinterpret_cast<USART_TypeDef *>(base)) {};

private:
    /// The buffers are shared with the ISR, keep the compiler from caching them across calls
    static inline void Barrier() { asm volatile("" ::: "memory"); }

    // IO base address
    USART_TypeDef *husart;

    CircularBuffer<uint8_t, uint_fast8_t, 32> tx_buf;
    CircularBuffer<uint8_t, uint_fast8_t, 32> rx_buf;
};

/// The USART connected to the printer
extern USART usart1;

} // namespace usart
} // namespace hal

namespace hu = hal::usart;
