/// @file tmc_uart.cpp
#include "tmc_uart.h"
#include "irq.h"
#include "cmsis.h"

namespace hal {
namespace tmc_uart {

static constexpr uint16_t bitTicks = (F_CPU + baudrate / 2) / baudrate;
/// Idle bit times before each datagram, lets the driver resynchronize
static constexpr int8_t idleBits = 2;
/// How long to wait for a start bit: SENDDELAY (8 bit times by default, 15 at most) plus margin
static constexpr uint8_t replyTimeoutBits = 32;

enum class Phase : uint8_t {
    Idle,
    Tx,
    RxWait,
    RxBits,
    Done,
    Timeout,
};

static volatile Phase phase = Phase::Idle;

static gpio::GPIO_TypeDef *port;
static uint16_t pinMask;
static uint8_t pinShift2; ///< MODER/PUPDR bit position of the pin

static const uint8_t *txData;
static uint8_t txLen;
static uint8_t *rxData;
static uint8_t rxLen;
static uint8_t byteIdx;
static int8_t bitIdx; ///< TX: <0 idle, 0 start, 1..8 data, 9 stop; RX: 0..7 data, 8 stop
static uint8_t shiftReg;
static uint8_t waitBits;

static inline void DriveHigh() { port->BSRR = pinMask; }
static inline void DriveLow() { port->BSRR = (uint32_t)pinMask << 16; }

/// Release the line: input with pull-up
static inline void Release() {
    port->MODER &= ~(3U << pinShift2);
}

static inline void StopTimer() {
    TIM17->CR1 = 0;
    TIM17->DIER = 0;
}

static inline void ArmStartBit() {
    EXTI->PR = pinMask;
    EXTI->IMR |= pinMask;
}

static inline void DisarmStartBit() {
    EXTI->IMR &= ~(uint32_t)pinMask;
    EXTI->PR = pinMask;
}

static void Finish(Phase result) {
    StopTimer();
    DisarmStartBit();
    Release();
    phase = result;
}

void Init() {
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
    (void)RCC->APB2ENR;
    StopTimer();
    TIM17->PSC = 0;

    NVIC_SetPriority(TIM17_IRQn, irq::prioTmcUart);
    NVIC_EnableIRQ(TIM17_IRQn);
    NVIC_SetPriority(EXTI0_1_IRQn, irq::prioTmcUart);
    NVIC_EnableIRQ(EXTI0_1_IRQn);
    NVIC_SetPriority(EXTI2_3_IRQn, irq::prioTmcUart);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
    NVIC_SetPriority(EXTI4_15_IRQn, irq::prioTmcUart);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
}

bool Transfer(const gpio::GPIO_pin pin, const uint8_t *tx, uint8_t txCount, uint8_t *rx, uint8_t rxCount) {
    if (__get_PRIMASK())
        return false; // the bits are timed by interrupts

    port = gpio::Regs(pin);
    pinMask = pin.pin_mask;
    const uint8_t pinNr = __builtin_ctz(pin.pin_mask);
    pinShift2 = pinNr * 2;

    txData = tx;
    txLen = txCount;
    rxData = rx;
    rxLen = rxCount;
    byteIdx = 0;
    bitIdx = -idleBits;

    {
        irq::CriticalSection cs;
        // route the pin to its EXTI line, falling edge (start bit)
        volatile uint32_t &exticr = SYSCFG->EXTICR[pinNr >> 2];
        const uint32_t shift4 = (pinNr & 3) * 4;
        const uint32_t portNr = (static_cast<uint32_t>(pin.port) - GPIOA_BASE) / (GPIOB_BASE - GPIOA_BASE);
        exticr = (exticr & ~(0xFU << shift4)) | (portNr << shift4);
        EXTI->IMR &= ~(uint32_t)pinMask;
        EXTI->RTSR &= ~(uint32_t)pinMask;
        EXTI->FTSR |= pinMask;
    }

    // idle high, push-pull output while sending
    gpio::WritePin(pin, gpio::Level::high);
    gpio::Configure(pin, 1, 1);

    phase = Phase::Tx;
    asm volatile("" ::: "memory"); // the ISR takes over the buffers from here
    TIM17->ARR = bitTicks - 1;
    TIM17->CNT = 0;
    TIM17->SR = 0;
    TIM17->DIER = TIM_DIER_UIE;
    TIM17->CR1 = TIM_CR1_CEN;

    while (phase != Phase::Done && phase != Phase::Timeout)
        ;
    asm volatile("" ::: "memory"); // ... and hands them back
    return phase == Phase::Done;
}

static void TxTick() {
    if (bitIdx < 0) {
        // idle
    } else if (bitIdx == 0) {
        shiftReg = txData[byteIdx];
        DriveLow();
    } else if (bitIdx <= 8) {
        if (shiftReg & 1)
            DriveHigh();
        else
            DriveLow();
        shiftReg >>= 1;
    } else if (bitIdx == 9) {
        DriveHigh();
    } else {
        // the stop bit is complete
        if (++byteIdx < txLen) {
            bitIdx = 0;
            shiftReg = txData[byteIdx];
            DriveLow();
            return;
        }
        if (rxLen == 0) {
            Finish(Phase::Done);
            return;
        }
        Release();
        byteIdx = 0;
        waitBits = 0;
        phase = Phase::RxWait;
        ArmStartBit();
        return;
    }
    ++bitIdx;
}

static void RxTick() {
    if (bitIdx == 0)
        TIM17->ARR = bitTicks - 1; // the first sample was 1.5 bits after the start edge
    const bool level = port->IDR & pinMask;
    if (bitIdx < 8) {
        shiftReg >>= 1;
        if (level)
            shiftReg |= 0x80;
        ++bitIdx;
        return;
    }
    // stop bit
    rxData[byteIdx] = shiftReg;
    if (++byteIdx == rxLen) {
        Finish(Phase::Done);
        return;
    }
    waitBits = 0;
    phase = Phase::RxWait;
    ArmStartBit();
}

static void TimerIsr() {
    TIM17->SR = ~TIM_SR_UIF;
    switch (phase) {
    case Phase::Tx:
        TxTick();
        break;
    case Phase::RxWait:
        if (++waitBits > replyTimeoutBits)
            Finish(Phase::Timeout);
        break;
    case Phase::RxBits:
        RxTick();
        break;
    default:
        StopTimer();
        break;
    }
}

static void StartBitIsr() {
    if (!(EXTI->PR & pinMask))
        return;
    DisarmStartBit();
    if (phase != Phase::RxWait)
        return;
    // sample in the middle of the first data bit, then every bit time
    TIM17->ARR = bitTicks + bitTicks / 2 - 1;
    TIM17->CNT = 0;
    TIM17->SR = 0;
    bitIdx = 0;
    shiftReg = 0;
    phase = Phase::RxBits;
}

} // namespace tmc_uart
} // namespace hal

extern "C" {

void TIM17_IRQHandler() {
    hal::tmc_uart::TimerIsr();
}

void EXTI0_1_IRQHandler() {
    hal::tmc_uart::StartBitIsr();
}

void EXTI2_3_IRQHandler() {
    hal::tmc_uart::StartBitIsr();
}

void EXTI4_15_IRQHandler() {
    hal::tmc_uart::StartBitIsr();
}

} // extern "C"
