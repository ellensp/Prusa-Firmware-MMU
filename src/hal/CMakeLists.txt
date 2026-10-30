if(MMU_BOARD STREQUAL "MMU2_DIP")
  target_sources(
    firmware
    PRIVATE stm32/startup.cpp
            stm32/cpu.cpp
            stm32/gpio.cpp
            stm32/usart.cpp
            stm32/shr16.cpp
            stm32/eeprom.cpp
            stm32/timers.cpp
            stm32/adc.cpp
            stm32/watchdog.cpp
    )
  if(MMU_DRIVER STREQUAL "TMC2209")
    target_sources(firmware PRIVATE tmc2209.cpp stm32/tmc_uart.cpp)
  else()
    target_sources(firmware PRIVATE tmc2130.cpp stm32/spi.cpp)
  endif()
else()
  target_sources(
    firmware
    PRIVATE avr/cpu.cpp
            avr/usart.cpp
            avr/shr16.cpp
            avr/eeprom.cpp
            avr/timers.cpp
            tmc2130.cpp
            adc.cpp
            avr/spi.cpp
            avr/watchdog.cpp
    )
endif()
