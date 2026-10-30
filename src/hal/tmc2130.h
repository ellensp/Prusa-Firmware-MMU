/// @file tmc2130.h
#pragma once
#include "../config/config.h"
#include "../hal/gpio.h"
#include "../hal/shr16.h"
#include "../hal/spi.h"

namespace hal {

/// TMC2130 interface
/// There are multiple TMC2130 on our board, so there will be multiple
/// instances of this class
namespace tmc2130 {

enum MotorMode : uint8_t {
    Stealth,
    Normal
};

struct MotorParams {
    const hal::spi::SPI_TypeDef *const spi;
    uint8_t idx; ///< SHR16 index
    bool dirOn; ///< forward direction
    gpio::GPIO_pin csPin; ///< CS pin
    gpio::GPIO_pin stepPin; ///< step pin
    gpio::GPIO_pin sgPin; ///< StallGuard pin
    config::MRes mRes; ///< microstep resolution
    int8_t sg_thrs;
    uint8_t axis;
};

struct MotorCurrents {
    bool vSense; ///< VSense current scaling
    uint8_t iRun; ///< Running current
    uint8_t iHold; ///< Holding current

    constexpr MotorCurrents(uint8_t ir, uint8_t ih)
        : vSense((ir < 32) ? 1 : 0)
        , iRun((ir < 32) ? ir : (ir >> 1))
        , iHold((ir < 32) ? ih : (ih >> 1)) {}
};

struct __attribute__((packed)) ErrorFlags {
    uint8_t reset_flag : 1; ///< driver restarted
    uint8_t uv_cp : 1; ///< undervoltage on charge pump
    uint8_t s2g : 1; ///< short to ground
    uint8_t otpw : 1; ///< over temperature pre-warning
    uint8_t ot : 1; ///< over temperature hard
    inline constexpr ErrorFlags()
        : reset_flag(0)
        , uv_cp(0)
        , s2g(0)
        , otpw(0)
        , ot(0) {}
    inline bool Good() const { return reset_flag == 0 && uv_cp == 0 && s2g == 0 && otpw == 0 && ot == 0; }
};

/// Current settings as written into the driver
struct DriverCurrents {
    bool vSense;
    uint8_t iRun;
    uint8_t iHold;
};

#ifdef MMU_BOARD_MMU2_DIP
/// Sense resistors of the Original Prusa MMU board, derived from the current table in tmc2130.cpp (31 -> 530mA)
static constexpr uint16_t prusaRSenseMilliOhm = 220;

/// Rescale one current register value from the Original Prusa MMU board to the board's sense resistors.
/// I = (cs + 1) / 32 * Vfs / (Rsense + 20mOhm) / sqrt(2), with Vfs = 180mV (vSense) or 320mV
/// @returns cs + 1 in vSense units, which may exceed 32
static constexpr uint16_t ScaledCurrent(bool vSense, uint8_t cs) {
    return ((uint32_t)(cs + 1) * (vSense ? 180U : 320U) * (config::tmcRSenseMilliOhm + 20U) + 180U * (prusaRSenseMilliOhm + 20U) / 2)
        / (180U * (prusaRSenseMilliOhm + 20U));
}

/// Register value for a scaled current
static constexpr uint8_t CurrentRegister(bool vSense, uint16_t scaled) {
    if (!vSense) // convert to the 320mV range
        scaled = (scaled * 180U + 160U) / 320U;
    return scaled == 0 ? 0 : (scaled > 32 ? 31 : scaled - 1);
}

/// @returns the currents for the drivers on this board
static constexpr DriverCurrents ForDriver(const MotorCurrents &currents) {
    // stay in the more precise 180mV range if the running current fits
    const bool vSense = ScaledCurrent(currents.vSense, currents.iRun) <= 32;
    return { vSense,
        CurrentRegister(vSense, ScaledCurrent(currents.vSense, currents.iRun)),
        CurrentRegister(vSense, ScaledCurrent(currents.vSense, currents.iHold)) };
}

/// The DIAG outputs are push-pull, active high (DIAG1 of the TMC2130, DIAG of the TMC2209)
static constexpr gpio::Level stallLevel = gpio::Level::high;
static constexpr gpio::Pull diagPull = gpio::Pull::down;
#else
static constexpr DriverCurrents ForDriver(const MotorCurrents &currents) {
    return { currents.vSense, currents.iRun, currents.iHold };
}

/// DIAG0 is open collector, active low with external pullups
static constexpr gpio::Level stallLevel = gpio::Level::low;
static constexpr gpio::Pull diagPull = gpio::Pull::up;
#endif

/// TMC2130 interface - instances of this class are hidden in modules::motion::Motion::AxisData
class TMC2130 {
public:
    /// TMC2130 register addresses
    enum class Registers : uint8_t {
        /// General Configuration Registers
        GCONF = 0x00,
        GSTAT = 0x01,
#ifdef MMU_DRIVER_TMC2209
        IFCNT = 0x02,
        IOIN = 0x06,
#else
        IOIN = 0x04,
#endif

        /// Velocity Dependent Driver Feature Control Register Set
        IHOLD_IRUN = 0x10,
        TPOWERDOWN = 0x11,
        TSTEP = 0x12,
        TPWMTHRS = 0x13,
        TCOOLTHRS = 0x14,
#ifdef MMU_DRIVER_TMC2209
        /// StallGuard Control Register Set
        SGTHRS = 0x40,
        SG_RESULT = 0x41,
        COOLCONF = 0x42,

        /// Motor Driver Registers
        MSCNT = 0x6A,
        CHOPCONF = 0x6C,
        DRV_STATUS = 0x6F,
        PWMCONF = 0x70,
#else
        THIGH = 0x15,

        /// Motor Driver Registers
        MSCNT = 0x6A,
        CHOPCONF = 0x6C,
        COOLCONF = 0x6D,
        DRV_STATUS = 0x6F,
        PWMCONF = 0x70,
#endif
    };

    /// Constructor
    TMC2130() = default;

    /// (re)initialization of the chip - please note this is necessary due to some HW flaws in the original MMU boards.
    /// And yes, the TMC may not get correctly initialized.
    /// @returns true if the TMC2130 was inited correctly
    bool Init(const MotorParams &params,
        const MotorCurrents &currents,
        MotorMode mode);

    /// Set the current motor mode
    void SetMode(const MotorParams &params, MotorMode mode);

    /// Disables the output by setting or clearing CHOPCONF's TOFF.
    void SetBridgeOutput(const MotorParams &params, bool on);

    /// Set the current motor currents
    void SetCurrents(const MotorParams &params, const MotorCurrents &currents);

    /// Set StallGuard threshold
    /// New SGTHRS must be already set in @p params
    /// Beware - there are no checks, new value is written into the TMC register immediately.
    /// It is advised to prefer setting the SGTHRS via the Init() method.
    void SetSGTHRS(const MotorParams &params);

    /// Return enabled state
    const bool Enabled() const {
        return enabled;
    }

    /// Enable/Disable the motor
    void SetEnabled(const MotorParams &params, bool enabled);

    /// Set direction
    static inline void SetDir(const MotorParams &params, bool dir) {
        hal::shr16::shr16.SetTMCDir(params.idx, dir ^ params.dirOn);
    }

    /// Set direction, raw value (i.e. ignore Params).
    static inline void SetRawDir(const MotorParams &params, bool dir) {
        // Also cancels the inversion in SetTMCDir
        hal::shr16::shr16.SetTMCDir(params.idx, !dir);
    }

    /// Step the motor
    static inline void Step(const MotorParams &params) {
        gpio::TogglePin(params.stepPin); // assumes DEDGE
    }

    /// Set step to an explicit state
    static inline void SetStep(const MotorParams &params, bool state) {
        gpio::WritePin(params.stepPin, (state ? gpio::Level::high : gpio::Level::low));
    }

    /// Return SG state
    static inline bool SampleDiag(const MotorParams &params) {
        return gpio::ReadPin(params.sgPin) == stallLevel;
    }

    /// Translate an IOIN register value into the STEP (bit 0), DIR (bit 1) and enabled (bit 2) pin states
    static inline uint8_t IOINToStepDirEna(uint32_t ioin) {
#ifdef MMU_DRIVER_TMC2209
        return ((ioin >> 7) & 1) | (((ioin >> 9) & 1) << 1) | ((ioin & 1) ? 0 : 4); // Note the logic inversion for ENA readback!
#else
        return (ioin & 0b11) | ((ioin & 0b10000) ? 0 : 4); // Note the logic inversion for ENA readback!
#endif
    }

    inline bool Stalled() const {
        return sg_filter_counter == 0;
    }

    inline void ClearStallguard() {
        sg_filter_counter = sg_filter_threshold;
    }

    /// Should be called periodically. Maybe not all the time. Once every 10 ms is probably enough
    bool CheckForErrors(const MotorParams &params);

    inline ErrorFlags GetErrorFlags() const {
        return errorFlags;
    }

#ifdef UNITTEST
    /// A very brutal way of fiddling with the error flags from the outside of this class
    /// Intended only for the UNIT TESTS!
    inline void SetErrorFlags(ErrorFlags ef) {
        errorFlags = ef;
    }
#endif

    /// Reads a driver register and updates the status flags
    uint32_t ReadRegister(const MotorParams &params, Registers reg);

    /// Writes a driver register and updates the status flags
    void WriteRegister(const MotorParams &params, Registers reg, uint32_t data);

    /// Used for polling the DIAG pin. Should be called from the stepper isr periodically when moving.
    void Isr(const MotorParams &params);

private:
    void _spi_tx_rx(const MotorParams &params, uint8_t (&pData)[5]);
    void _handle_spi_status(const MotorParams &params, uint8_t status);

    ErrorFlags errorFlags;
    bool initialized = false;
    bool enabled = false;
    uint8_t sg_filter_threshold = 0;
    uint8_t sg_filter_counter = 0;
};

} // namespace tmc2130
} // namespace hal
