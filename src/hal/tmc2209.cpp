/// @file tmc2209.cpp
/// TMC2209 behind the TMC2130 interface, for the MMU2-DIP.
///
/// Differences to the TMC2130 that matter here:
/// - registers go over a single-wire UART (see stm32/tmc_uart.h), not SPI
/// - StallGuard4 only works in StealthChop, so the drivers stay in StealthChop
///   in both modes, and SetMode() doesn't change anything
/// - SGTHRS is 0..255 and higher is more sensitive, the opposite of the TMC2130's SGT
/// - GSTAT is cleared by writing ones, not by reading it
#include "tmc2130.h"
#include "stm32/tmc_uart.h"
#include "../config/config.h"
#include "../modules/timebase.h"

namespace hal {
namespace tmc2130 {

static constexpr uint8_t TOFF_DEFAULT = 3U, TOFF_MASK = 0xFU;

/// UART address, set by MS1/MS2 - both low on the MMU2-DIP
static constexpr uint8_t uartAddress = 0;
static constexpr uint8_t syncByte = 0x05;
static constexpr uint8_t readRetries = 3;
/// IOIN version field of the TMC2209
static constexpr uint8_t version = 0x21;
/// CheckForErrors() costs two register reads over the UART - limit how often it actually talks to the driver
static constexpr uint16_t errorCheckIntervalMs = 50;

static uint16_t lastErrorCheckMs[config::NUM_AXIS];

/// CRC8 of the Trinamic UART datagrams: polynomial x^8 + x^2 + x + 1, LSB of each byte first
static uint8_t Crc(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            if ((crc >> 7) ^ (b & 0x01))
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
            b >>= 1;
        }
    }
    return crc;
}

bool __attribute__((noinline)) TMC2130::Init(const MotorParams &params, const MotorCurrents &currents, MotorMode mode) {
    initialized = false;

    sg_filter_threshold = 2;

    gpio::Init(params.sgPin, gpio::GPIO_InitTypeDef(gpio::Mode::input, diagPull));
    gpio::Init(params.stepPin, gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::low));

    /// check for a responding TMC2209 (IOIN version field)
    uint32_t IOIN = ReadRegister(params, Registers::IOIN);
    if ((IOIN >> 24U) != version)
        return false;

    /// clear reset_flag as we are (re)initializing
    errorFlags.reset_flag = false;

    // clear all error flags
    WriteRegister(params, Registers::GSTAT, 0x07);

    /// UART controls the microstepping and the power down, internal current reference, StealthChop
    constexpr uint32_t gconf = (uint32_t)(1U & 0x01U) << 6U // pdn_disable
        | (uint32_t)(1U & 0x01U) << 7U // mstep_reg_select
        | (uint32_t)(1U & 0x01U) << 8U; // multistep_filt
    WriteRegister(params, Registers::GCONF, gconf);

    /// apply chopper parameters - same layout and values as on the TMC2130
    const uint32_t chopconf = (uint32_t)(TOFF_DEFAULT & TOFF_MASK) << 0U // toff
        | (uint32_t)(5U & 0x07U) << 4U // hstrt
        | (uint32_t)(1U & 0x0FU) << 7U // hend
        | (uint32_t)(2U & 0x03U) << 15U // tbl
        | (uint32_t)(ForDriver(currents).vSense & 0x01U) << 17U // vsense
        | (uint32_t)(params.mRes & 0x0FU) << 24U // mres
        | (uint32_t)(1U & 0x01) << 28U // intpol
        | (uint32_t)(1U & 0x01) << 29U; // dedge
    WriteRegister(params, Registers::CHOPCONF, chopconf);

    /// apply currents
    SetCurrents(params, currents);

    /// the shortest power down delay that still lets StealthChop tune itself
    WriteRegister(params, Registers::TPOWERDOWN, 2);

    /// Stallguard parameters
    SetSGTHRS(params);
    WriteRegister(params, Registers::TCOOLTHRS, config::tmc2130_coolStepThreshold);
    WriteRegister(params, Registers::COOLCONF, 0); // no CoolStep

    /// stealthChop parameters, freewheeling as on the TMC2130
    constexpr uint32_t pwmconf = (uint32_t)36U << 0U // PWM_OFS
        | (uint32_t)14U << 8U // PWM_GRAD
        | (uint32_t)1U << 16U // pwm_freq
        | (uint32_t)1U << 18U // pwm_autoscale
        | (uint32_t)1U << 19U // pwm_autograd
        | (uint32_t)(config::tmc2130_freewheel & 0x03U) << 20U // freewheel
        | (uint32_t)8U << 24U // PWM_REG
        | (uint32_t)12U << 28U; // PWM_LIM
    WriteRegister(params, Registers::PWMCONF, pwmconf);

    SetMode(params, mode);

    initialized = true;
    return true;
}

void TMC2130::SetMode(const MotorParams &params, MotorMode mode) {
    /// StallGuard4 needs StealthChop - never switch to SpreadCycle
    WriteRegister(params, Registers::TPWMTHRS, 0);
}

void TMC2130::SetBridgeOutput(const MotorParams &params, bool bOn) {
    uint32_t chopconf = ReadRegister(params, Registers::CHOPCONF);
    chopconf &= ~((uint32_t)TOFF_MASK);
    if (bOn) {
        chopconf |= TOFF_DEFAULT;
    }
    WriteRegister(params, Registers::CHOPCONF, chopconf);
}

void TMC2130::SetCurrents(const MotorParams &params, const MotorCurrents &currents) {
    const DriverCurrents c = ForDriver(currents);
    // make sure iHold never exceeds iRun at runtime
    const uint8_t iHold = c.iHold > c.iRun ? c.iRun : c.iHold;
    const uint32_t ihold_irun = (uint32_t)(iHold & 0x1F) << 0 // ihold
        | (uint32_t)(c.iRun & 0x1F) << 8 // irun
        | (uint32_t)(15 & 0x0F) << 16; // IHOLDDELAY
    WriteRegister(params, Registers::IHOLD_IRUN, ihold_irun);
}

void TMC2130::SetSGTHRS(const MotorParams &params) {
    WriteRegister(params, Registers::SGTHRS, (uint8_t)params.sg_thrs);
}

void TMC2130::SetEnabled(const MotorParams &params, bool enabled) {
    hal::shr16::shr16.SetTMCEnabled(params.idx, enabled);
    if (this->enabled != enabled)
        ClearStallguard();
    this->enabled = enabled;
}

bool __attribute__((noinline)) TMC2130::CheckForErrors(const MotorParams &params) {
    if (!initialized)
        return false;

    const uint16_t now = mt::timebase.Millis();
    if ((uint16_t)(now - lastErrorCheckMs[params.axis]) < errorCheckIntervalMs)
        return false;
    lastErrorCheckMs[params.axis] = now;

    uint32_t GSTAT = ReadRegister(params, Registers::GSTAT);
    uint32_t DRV_STATUS = ReadRegister(params, Registers::DRV_STATUS);
    if (GSTAT)
        WriteRegister(params, Registers::GSTAT, GSTAT); // clear what we've seen
    errorFlags.reset_flag |= !!(GSTAT & (1U << 0U));
    errorFlags.uv_cp = !!(GSTAT & (1U << 2U));
    errorFlags.s2g = !!(DRV_STATUS & (0xFUL << 2U)); // s2ga, s2gb, s2vsa, s2vsb
    errorFlags.otpw = !!(DRV_STATUS & (1UL << 0U));
    errorFlags.ot = !!(DRV_STATUS & (1UL << 1U));

    return GSTAT || errorFlags.reset_flag; // any bit in gstat is an error
}

uint32_t TMC2130::ReadRegister(const MotorParams &params, Registers reg) {
    uint8_t request[4] = { syncByte, uartAddress, (uint8_t)reg, 0 };
    request[3] = Crc(request, 3);
    uint8_t reply[8];
    for (uint8_t i = 0; i < readRetries; ++i) {
        if (tmc_uart::Transfer(params.csPin, request, sizeof(request), reply, sizeof(reply))
            && reply[0] == syncByte && reply[1] == 0xFF && reply[2] == (uint8_t)reg && reply[7] == Crc(reply, 7)) {
            return (uint32_t)reply[3] << 24 | (uint32_t)reply[4] << 16 | (uint32_t)reply[5] << 8 | (uint32_t)reply[6];
        }
    }
    return 0;
}

void TMC2130::WriteRegister(const MotorParams &params, Registers reg, uint32_t data) {
    uint8_t datagram[8] = { syncByte, uartAddress, (uint8_t)((uint8_t)(reg) | 0x80), (uint8_t)(data >> 24), (uint8_t)(data >> 16), (uint8_t)(data >> 8), (uint8_t)data, 0 };
    datagram[7] = Crc(datagram, 7);
    tmc_uart::Transfer(params.csPin, datagram, sizeof(datagram), nullptr, 0);
}

void TMC2130::Isr(const MotorParams &params) {
    if (sg_filter_counter) {
        if (SampleDiag(params))
            sg_filter_counter--;
        else if (sg_filter_counter < sg_filter_threshold)
            sg_filter_counter++;
    }
}

} // namespace tmc2130
} // namespace hal
