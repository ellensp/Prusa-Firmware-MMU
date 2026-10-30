# MMU3 firmware for the BIGTREETECH MMU2-DIP

This branch runs the current Prusa MMU firmware (the MMU3 firmware, which also drives MMU2S
mechanics) on the [BIGTREETECH MMU2-DIP](https://github.com/bigtreetech/MMU2-DIP) controller.
The board replaces the Original Prusa MMU control board. It has an STM32F030C8 instead of the
ATmega32U4, and StepStick sockets instead of on-board TMC2130 drivers.

> **Status:** both driver variants compile and link. Nothing has been run on an MMU2-DIP yet.
> Treat the first flash as a bench test, with the motors free to move and nothing loaded.

## Building

You need CMake (3.19 or newer) and an `arm-none-eabi-gcc` toolchain on your `PATH`.

```
cmake -S . -B build-dip -DCMAKE_TOOLCHAIN_FILE=cmake/ArmGcc.cmake -DMMU_BOARD=MMU2_DIP -DMMU_DRIVER=TMC2209 -DCMAKE_BUILD_TYPE=Release
cmake --build build-dip
```

Set `-DMMU_DRIVER=TMC2130` if the board has TMC2130 drivers fitted in SPI mode.
The build writes `MMU2_DIP_<driver>_FW<version>.hex` and `.bin` into the build directory.

Without `MMU_BOARD`, the project builds for the Original Prusa MMU board exactly as before.

## Flashing

The board has no USB bootloader of its own. Flash through the STM32's built-in serial bootloader
over the on-board CH340, as described in BIGTREETECH's MMU2-DIP manual: FlyMcu on Windows, or
`stm32flash` elsewhere (`stm32flash -w MMU2_DIP_TMC2209_FW<version>.bin -v -g 0x0 /dev/ttyUSB0`).

The last 4 KB of flash hold the settings: bowden length, StallGuard thresholds, the loaded
filament. A full-chip erase resets them. Holding the left button at power-up also resets them,
just as on the Original Prusa MMU board.

## Connecting to the printer

The firmware speaks the MMU3 protocol on USART1 (the printer connector) at 115200 baud.
In Marlin, use `#define MMU_MODEL PRUSA_MMU3` with `MMU_BAUD 115200`.

The CH340 is wired to the same USART as the printer connector, so the USB port is only for
flashing. The firmware has no USB serial console.

## Stepper drivers

| Axis     | Driver (schematic) | STEP | UART / CS | DIAG |
| -------- | ------------------ | ---- | --------- | ---- |
| Pulley   | M1 / E1            | PB4  | PB1       | PB7  |
| Selector | M2 / E2            | PA11 | PB2       | PB6  |
| Idler    | M3 / E3            | PA8  | PB10      | PB5  |

DIR and EN go through the two 74HC595s, wired the same way as on the Prusa board.

**TMC2209:** jumper the sockets for UART mode as described in BIGTREETECH's manual, with the
driver address at 0 (MS1 and MS2 low). Connect the DIAG pins, because selector and idler homing
depend on StallGuard.

**TMC2130:** jumper the sockets for SPI. The firmware uses the DIAG1 output, push-pull and
active high.

### What behaves differently from an Original Prusa MMU

- **Motor current.** `iRun`/`iHold` in `src/config/config.h` are register values for the Prusa
  board's 0.22 Ω sense resistors. BIGTREETECH's StepStick modules use 0.11 Ω, so the drivers
  rescale every value to give the same current. If your modules have different sense
  resistors, change `config::tmcRSenseMilliOhm`.
- **TMC2209 runs in StealthChop all the time.** Its StallGuard4 only works in StealthChop, so
  the stealth/normal mode switch has no effect on these drivers.
- **StallGuard thresholds.** The TMC2209 threshold (SGTHRS, 0–255) works the opposite way to
  the TMC2130's: higher means more sensitive. The defaults are 50 for the selector and 20 for
  the idler, taken from BIGTREETECH's firmware, and will likely need tuning for your
  mechanics. Change them from the printer with `M708 A0x18 Xnn` (selector) and
  `M708 A0x19 Xnn` (idler). The firmware saves them.
- **Motor direction.** BIGTREETECH's firmware turns all three motors the opposite way to Prusa's
  MM-control-01, so `config::dirInvert` flips them here too. If an axis still runs backwards,
  change `.dirOn` for that axis in `config.h`.
- **Buttons and undervoltage detection** are recalibrated for the 12-bit ADC and the 3.3 V
  supply. The MMU reports undervoltage when the supply drops below about 3.0 V.

## How the port is put together

The application logic and the modules are untouched apart from a few hooks. The port adds:

- `src/hal/stm32/`: the STM32F030 implementations of the HAL (clock, GPIO, USART1, the shift
  register, the 2 MHz step timer on TIM3, the SysTick timebase, the ADC, the IWDG watchdog), plus
  the startup code and linker script.
- `src/hal/stm32/eeprom.cpp`: 1 KB of EEPROM emulated in flash: a RAM mirror, plus two 2 KB
  banks of full snapshot and change log. Erasing a flash page stalls the CPU, so the main loop
  compacts the log while the motors stand still.
- `src/hal/tmc2209.cpp` and `src/hal/stm32/tmc_uart.cpp`: the TMC2209 behind the existing
  TMC2130 driver interface, on an interrupt-timed single-wire UART at 115200 baud.
- `lib/cmsis_stm32f0/`: CMSIS headers from Arm and ST (Apache 2.0).
- `cmake/ArmGcc.cmake`, plus the `MMU_BOARD`/`MMU_DRIVER` options in `CMakeLists.txt`.
