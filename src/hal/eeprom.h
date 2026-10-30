/// @file eeprom.h
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace hal {

/// EEPROM interface
namespace eeprom {

class EEPROM {
public:
#if defined(UNITTEST) || defined(__STM32__)
    using addr_t = size_t;
#else
    using addr_t = uint16_t;
#endif

    static void WriteByte(addr_t addr, uint8_t value);
    static void UpdateByte(addr_t addr, uint8_t value);
    static uint8_t ReadByte(addr_t addr);

    /// Convenience function to read a 1-byte value from EEPROM and check for unitialized EEPROM cells.
    /// @returns 1-byte value read from the EEPROM.
    /// In case the EEPROM has a default value at @p addr, this function returns @p defaultValue
    static uint8_t ReadByte(addr_t addr, uint8_t defaultValue);

    static void WriteWord(addr_t addr, uint16_t value);
    static void UpdateWord(addr_t addr, uint16_t value);
    static uint16_t ReadWord(addr_t addr);

    /// @returns physical end address of EEPROM memory end
    /// TODO this is sad - the constexpr must be inline... find a way around in the future
#ifdef __STM32__
    constexpr static addr_t End() { return 1023; } // 1 KB emulated in flash

    /// Housekeeping of the flash emulation. It may erase flash pages, which stalls the CPU
    /// for tens of ms - only call it while the motors are idle.
    static void Maintain();
#else
    constexpr static addr_t End() { return 2048; }
#endif
};

extern EEPROM eeprom;

} // namespace EEPROM
} // namespace hal
