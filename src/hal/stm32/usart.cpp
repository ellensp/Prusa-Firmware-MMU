/// @file usart.cpp
#include "../usart.h"
#include "cmsis.h"
#include "irq.h"

namespace hal {
namespace usart {

USART usart1(USART1_BASE);

/// The CMSIS view of the registers
static inline ::USART_TypeDef *Regs(USART::USART_TypeDef *husart) {
    return reinterpret_cast<::USART_TypeDef *>(husart);
}

void USART::Init(USART_InitTypeDef *const conf) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    // PA9/PA10 = USART1 TX/RX, AF1
    gpio::Configure(conf->tx_pin, 2, 0, 1);
    gpio::Configure(conf->rx_pin, 2, 1, 1);

    Regs(husart)->CR1 = 0;
    Regs(husart)->BRR = (F_CPU + conf->baudrate / 2) / conf->baudrate;
    Regs(husart)->CR3 = USART_CR3_OVRDIS; // an overrun just drops bytes, the protocol recovers
    // the receiver gets enabled later by rx_enable(), just like on the AVR
    Regs(husart)->CR1 = USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, irq::prioUsart);
    NVIC_EnableIRQ(USART1_IRQn);
}

void USART::rx_enable() {
    Regs(husart)->CR1 |= USART_CR1_RE;
}

uint8_t USART::Read() {
    uint8_t c = 0;
    Barrier();
    rx_buf.pop(c);
    Barrier();
    return c;
}

void USART::Write(uint8_t c) {
    // wait for the interrupt handler to make some space for us
    // (or drain the buffer ourselves if interrupts are disabled)
    for (;;) {
        Barrier();
        if (tx_buf.push(c))
            break;
        if (__get_PRIMASK() && (Regs(husart)->ISR & USART_ISR_TXE)) {
            ISR();
        }
    }
    Barrier();
    Regs(husart)->CR1 |= USART_CR1_TXEIE;
}

void USART::Flush() {
    for (;;) {
        Barrier();
        if (tx_buf.empty() && (Regs(husart)->ISR & USART_ISR_TC))
            break;
        if (__get_PRIMASK() && (Regs(husart)->ISR & USART_ISR_TXE)) {
            ISR();
        }
    }
}

void USART::WriteS(const char *str) {
    while (*str != 0) {
        Write(*str++);
    }
}

void USART::puts(const char *str) {
    WriteS(str);
    Write('\n');
}

void USART::ISR() {
    const uint32_t isr = Regs(husart)->ISR;
    if (isr & USART_ISR_RXNE) {
        // reading RDR clears RXNE
        const uint8_t c = Regs(husart)->RDR;
        if (!(isr & USART_ISR_FE))
            rx_buf.push(c);
    }
    if (isr & (USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE))
        Regs(husart)->ICR = USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF;

    if ((Regs(husart)->CR1 & USART_CR1_TXEIE) && (isr & USART_ISR_TXE)) {
        uint8_t c = 0;
        if (tx_buf.pop(c))
            Regs(husart)->TDR = c;
        if (tx_buf.empty())
            Regs(husart)->CR1 &= ~USART_CR1_TXEIE; // disable TXE interrupt
    }
}

} // namespace usart
} // namespace hal

extern "C" void USART1_IRQHandler() {
    hu::usart1.ISR();
}
