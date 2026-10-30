/// @file eeprom.cpp
/// EEPROM emulated in the last 4 KB of flash.
///
/// The flash holds two 2 KB banks, one of them active:
/// - header: generation (half-word, written first), magic (half-word, written last)
/// - snapshot: a full 1 KB image of the EEPROM at the time the bank became active
/// - log: 255 records of { address, value | ~value << 8 }, appended on every changed byte
///
/// A RAM mirror serves all reads. When the log fills up, the mirror is written as the snapshot
/// of the other bank, which then becomes active, and the old bank gets erased.
/// Erasing stalls the CPU (and with it the stepping) for tens of ms, so the main loop calls
/// Maintain() to do that early while the motors are idle.
#include "../eeprom.h"
#include "cmsis.h"
#include <string.h>

extern "C" uint32_t _eeprom_start;

namespace hal {
namespace eeprom {

EEPROM eeprom;

static constexpr uint32_t pageSize = 1024;
static constexpr uint32_t bankSize = 2 * pageSize;
static constexpr uint32_t eepromSize = EEPROM::End() + 1;
static constexpr uint32_t snapshotOffset = 4;
static constexpr uint32_t logOffset = snapshotOffset + eepromSize;
static constexpr uint16_t bankMagic = 0xEE01;
static constexpr uint16_t freeHalfWord = 0xFFFF;
/// Maintain() compacts once the log is half full
static constexpr uint32_t maintainOffset = logOffset + (bankSize - logOffset) / 2;

static_assert(logOffset < bankSize, "the snapshot doesn't fit into a bank");

static uint8_t mirror[eepromSize];
static bool loaded = false;
static uint8_t activeBank;
static uint16_t generation;
static uint32_t writeOffset; ///< offset of the first free log record in the active bank

static inline uint32_t BankAddr(uint8_t bank) {
    return reinterpret_cast<uint32_t>(&_eeprom_start) + bank * bankSize;
}

static inline uint16_t Read16(uint32_t addr) {
    return *reinterpret_cast<const volatile uint16_t *>(addr);
}

static void Unlock() {
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

static void Lock() {
    FLASH->CR |= FLASH_CR_LOCK;
}

static void WaitDone() {
    while (FLASH->SR & FLASH_SR_BSY)
        ;
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPERR;
}

static void Program16(uint32_t addr, uint16_t value) {
    FLASH->CR = FLASH_CR_PG;
    *reinterpret_cast<volatile uint16_t *>(addr) = value;
    WaitDone();
    FLASH->CR = 0;
}

static bool BankErased(uint8_t bank) {
    for (uint32_t a = BankAddr(bank); a < BankAddr(bank) + bankSize; a += 4) {
        if (*reinterpret_cast<const volatile uint32_t *>(a) != 0xFFFFFFFFU)
            return false;
    }
    return true;
}

static void EraseBank(uint8_t bank) {
    if (BankErased(bank))
        return;
    for (uint32_t page = 0; page < bankSize; page += pageSize) {
        FLASH->CR = FLASH_CR_PER;
        FLASH->AR = BankAddr(bank) + page;
        FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
        WaitDone();
        FLASH->CR = 0;
    }
}

static bool BankValid(uint8_t bank) {
    return Read16(BankAddr(bank)) == bankMagic && Read16(BankAddr(bank) + 2) != freeHalfWord;
}

/// Write the mirror as the snapshot of the other bank, make it active and erase the old one
static void Compact() {
    const uint8_t newBank = activeBank ^ 1;
    uint16_t newGeneration = generation + 1;
    if (newGeneration == freeHalfWord)
        newGeneration = 0;

    Unlock();
    EraseBank(newBank); // normally erased already, unless a compaction got interrupted
    const uint32_t base = BankAddr(newBank);
    for (uint32_t i = 0; i < eepromSize; i += 2) {
        const uint16_t v = mirror[i] | (uint16_t)mirror[i + 1] << 8;
        if (v != freeHalfWord)
            Program16(base + snapshotOffset + i, v);
    }
    Program16(base + 2, newGeneration);
    Program16(base, bankMagic);
    EraseBank(activeBank);
    Lock();

    activeBank = newBank;
    generation = newGeneration;
    writeOffset = logOffset;
}

static void Load() {
    loaded = true;

    const bool valid0 = BankValid(0), valid1 = BankValid(1);
    if (valid0 && valid1) {
        // a compaction got interrupted after activating the new bank - take the newer one
        activeBank = (int16_t)(Read16(BankAddr(1) + 2) - Read16(BankAddr(0) + 2)) > 0 ? 1 : 0;
    } else if (valid0 || valid1) {
        activeBank = valid1 ? 1 : 0;
    } else {
        // blank or corrupted - start over with an empty EEPROM
        memset(mirror, 0xff, sizeof(mirror));
        Unlock();
        EraseBank(0);
        EraseBank(1);
        Program16(BankAddr(0) + 2, 0);
        Program16(BankAddr(0), bankMagic);
        Lock();
        activeBank = 0;
        generation = 0;
        writeOffset = logOffset;
        return;
    }

    const uint32_t base = BankAddr(activeBank);
    generation = Read16(base + 2);
    memcpy(mirror, reinterpret_cast<const void *>(base + snapshotOffset), sizeof(mirror));

    for (writeOffset = logOffset; writeOffset < bankSize; writeOffset += 4) {
        const uint16_t addr = Read16(base + writeOffset);
        if (addr == freeHalfWord)
            break;
        const uint16_t data = Read16(base + writeOffset + 2);
        // skip records torn by a reset
        if (addr < eepromSize && (uint8_t)(data >> 8) == (uint8_t)~data)
            mirror[addr] = (uint8_t)data;
    }

    // finish what an interrupted compaction left behind
    if (!BankErased(activeBank ^ 1)) {
        Unlock();
        EraseBank(activeBank ^ 1);
        Lock();
    }
}

static inline void EnsureLoaded() {
    if (!loaded)
        Load();
}

static void Append(uint16_t addr, uint8_t value) {
    if (writeOffset >= bankSize)
        Compact(); // the mirror already holds the new value
    else {
        const uint32_t record = BankAddr(activeBank) + writeOffset;
        Unlock();
        Program16(record, addr);
        Program16(record + 2, value | (uint16_t)(uint8_t)~value << 8);
        Lock();
        writeOffset += 4;
    }
}

uint8_t EEPROM::ReadByte(EEPROM::addr_t addr) {
    EnsureLoaded();
    return addr < eepromSize ? mirror[addr] : 0xff;
}

uint8_t EEPROM::ReadByte(EEPROM::addr_t addr, uint8_t defaultValue) {
    uint8_t v = ReadByte(addr);
    return v == 0xff ? defaultValue : v;
}

void EEPROM::UpdateByte(EEPROM::addr_t addr, uint8_t value) {
    EnsureLoaded();
    if (addr >= eepromSize || mirror[addr] == value)
        return;
    mirror[addr] = value;
    Append(addr, value);
}

uint16_t EEPROM::ReadWord(EEPROM::addr_t addr) {
    return ReadByte(addr) | (uint16_t)ReadByte(addr + 1) << 8;
}

void EEPROM::UpdateWord(EEPROM::addr_t addr, uint16_t value) {
    UpdateByte(addr, (uint8_t)value);
    UpdateByte(addr + 1, (uint8_t)(value >> 8));
}

void EEPROM::Maintain() {
    if (loaded && writeOffset >= maintainOffset)
        Compact();
}

} // namespace eeprom
} // namespace hal
