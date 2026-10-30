cmake_minimum_required(VERSION 3.19)
include(cmake/Utilities.cmake)
include(cmake/GetGitRevisionDescription.cmake)
include(cmake/ProjectVersion.cmake)
include(cmake/ReproducibleBuild.cmake)

project(
  MMU
  LANGUAGES C CXX ASM
  VERSION ${PROJECT_VERSION}
  )

#
# Command Line Options
#
# You should specify those options when invoking CMake. Example:
# ~~~
# cmake .. <other options> -DCUSTOM_COMPILE_OPTIONS=-DENABLE_FEATURE_X
# ~~~

set(PROJECT_VERSION_SUFFIX
    "<auto>"
    CACHE
      STRING
      "Full version suffix to be shown on the info screen in settings (e.g. full_version=4.0.3-BETA+1035.PR111.B4, suffix=-BETA+1035.PR111.B4). Defaults to '+<commit sha>.<dirty?>.<debug?>' if set to '<auto>'."
    )
set(PROJECT_VERSION_SUFFIX_SHORT
    "<auto>"
    CACHE
      STRING
      "Short version suffix to be shown on splash screen. Defaults to '+<BUILD_NUMBER>' if set to '<auto>'."
    )
set(BUILD_NUMBER
    ""
    CACHE STRING "Build number of the firmware. Resolved automatically if not specified."
    )
set(PROJECT_VERSION_TIMESTAMP
    ""
    CACHE STRING "Timestamp for the build. Resolved automatically if not specified."
    )
set(CUSTOM_COMPILE_OPTIONS
    ""
    CACHE STRING "Allows adding custom C/C++ flags"
    )
set(MMU_BOARD
    "MM_CONTROL"
    CACHE
      STRING
      "Target board: MM_CONTROL (Original Prusa MMU, ATmega32u4) or MMU2_DIP (BIGTREETECH MMU2-DIP, STM32F030C8)"
    )
set_property(CACHE MMU_BOARD PROPERTY STRINGS MM_CONTROL MMU2_DIP)
set(MMU_DRIVER
    "TMC2209"
    CACHE STRING "Stepper drivers fitted to the MMU2-DIP: TMC2209 (UART) or TMC2130 (SPI)"
    )
set_property(CACHE MMU_DRIVER PROPERTY STRINGS TMC2209 TMC2130)

# Resolve BUILD_NUMBER and PROJECT_VERSION_* variables
resolve_version_variables()

if(MMU_BOARD STREQUAL "MMU2_DIP")
  if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "MMU_BOARD=MMU2_DIP needs -DCMAKE_TOOLCHAIN_FILE=cmake/ArmGcc.cmake")
  endif()
  if(NOT MMU_DRIVER MATCHES "^(TMC2209|TMC2130)$")
    message(FATAL_ERROR "MMU_DRIVER must be TMC2209 or TMC2130, not '${MMU_DRIVER}'")
  endif()
  add_compile_definitions(
    MMU_BOARD_MMU2_DIP __STM32__ STM32F030x8 F_CPU=48000000UL MMU_DRIVER_${MMU_DRIVER}
    )
  message(STATUS "Target board: BIGTREETECH MMU2-DIP with ${MMU_DRIVER} drivers")
elseif(NOT MMU_BOARD STREQUAL "MM_CONTROL")
  message(FATAL_ERROR "Unknown MMU_BOARD '${MMU_BOARD}'")
endif()

add_compile_definitions(
  PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PROJECT_VERSION_REV=${PROJECT_VERSION_REV}
  PROJECT_BUILD_NUMBER=${BUILD_NUMBER}
  FW_VERSION_FULL_STR="${PROJECT_VERSION_FULL}"
  FW_VERSION_STR="${PROJECT_VERSION}"
  FW_VERSION_SUFFIX_STR="${PROJECT_VERSION_SUFFIX}"
  FW_VERSION_SUFFIX_SHORT_STR="${PROJECT_VERSION_SUFFIX_SHORT}"
  )

# Check GCC Version
get_recommended_gcc_version(RECOMMENDED_TOOLCHAIN_VERSION)
if(MMU_BOARD STREQUAL "MMU2_DIP")
  # any recent arm-none-eabi-gcc will do
elseif(CMAKE_CROSSCOMPILING AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL
                            ${RECOMMENDED_TOOLCHAIN_VERSION}
   )
  message(WARNING "Recommended AVR toolchain is ${RECOMMENDED_TOOLCHAIN_VERSION}"
                  ", but you have ${CMAKE_CXX_COMPILER_VERSION}"
          )

elseif(NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(
    WARNING
      "Recommended compiler for host tools and unittests is GCC, you have ${CMAKE_CXX_COMPILER_ID}."
    )
endif()

# Inform user about the resolved settings
message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Project version with full suffix: ${PROJECT_VERSION_FULL}")
message(
  STATUS "Project version with short suffix: ${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}"
  )
message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}.")

# eclipse sets those variables, so lets just use them so we don't get a warning about unused
# variables
set(unused "${CMAKE_VERBOSE_MAKEFILE} ${CMAKE_RULE_MESSAGES}")

# append custom C/C++ flags
if(CUSTOM_COMPILE_OPTIONS)
  string(REPLACE " " ";" CUSTOM_COMPILE_OPTIONS "${CUSTOM_COMPILE_OPTIONS}")
  add_compile_options(${CUSTOM_COMPILE_OPTIONS})
endif()

#
# Global Compiler & Linker Configuration
#

# include symbols
add_compile_options(-g)

# optimizations
if(CMAKE_CROSSCOMPILING)
  # set source epoch
  set_source_epoch(${PROJECT_VERSION_TIMESTAMP})

  # default optimization flags
  set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
  set(CMAKE_CXX_FLAGS_RELEASE "-Os -g -DNDEBUG")
  set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
  set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

  if(MMU_BOARD STREQUAL "MMU2_DIP")
    set(MCU_FLAGS -mcpu=cortex-m0 -mthumb)
    add_compile_options(${MCU_FLAGS})
    add_link_options(${MCU_FLAGS} --specs=nano.specs --specs=nosys.specs -nostartfiles)
  else()
    # mcu related settings: It would be nice to add "-mrelax" but due to some avr-gcc/linker bug it
    # breaks functionality of the FW (LoadFilament stopped working and only return Finished) The
    # bad news is that -mrelax was saving ~450B of CPU FLASH
    set(MCU_FLAGS -mmcu=atmega32u4 -DF_CPU=16000000L)
    add_compile_options(${MCU_FLAGS})
    add_link_options(${MCU_FLAGS})
  endif()

  # disable some C++ language features
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

  # disable exceptions
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>)

  # split and gc sections
  add_compile_options(-ffunction-sections -fdata-sections)
  add_link_options(-ffunction-sections -fdata-sections -Wl,--gc-sections)
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0)
  else()
    add_compile_options(-O2)
  endif()
endif()

# enable all warnings (well, not all, but some)
add_compile_options(-Wall -Wsign-compare)

# support _DEBUG macro (some code uses to recognize debug builds)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(_DEBUG)
endif()

#
# Import definitions of all libraries
#

add_subdirectory(lib)

#
# MMU firmware
#

add_executable(firmware)

set_target_properties(firmware PROPERTIES CXX_STANDARD 17)
set_target_properties(firmware PROPERTIES INTERPROCEDURAL_OPTIMIZATION True)

if(CMAKE_CROSSCOMPILING AND MMU_BOARD STREQUAL "MMU2_DIP")
  set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/stm32/stm32f030x8.ld)
  target_link_options(firmware PUBLIC -Wl,-T,${LINKER_SCRIPT} -Wl,-Map=firmware.map)
  add_link_dependency(firmware ${LINKER_SCRIPT})

  set(DIP_FW_NAME "MMU2_DIP_${MMU_DRIVER}_FW${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}")
  add_custom_command(
    TARGET firmware
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex firmware "${DIP_FW_NAME}.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary firmware "${DIP_FW_NAME}.bin"
    COMMAND ${CMAKE_OBJDUMP} --prefix ${CMAKE_SOURCE_DIR} -CSd firmware > firmware.asm
    BYPRODUCTS "${DIP_FW_NAME}.hex" "${DIP_FW_NAME}.bin" firmware.asm
    )
  report_size(firmware)

elseif(CMAKE_CROSSCOMPILING)
  # configure linker script
  set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/avr5.xn)
  target_link_options(firmware PUBLIC -Wl,-T,${LINKER_SCRIPT})
  add_link_dependency(firmware ${LINKER_SCRIPT})

  # limit the text section to 28K (32K - 4k reserved for the bootloader)
  target_link_options(firmware PUBLIC -Wl,--defsym=__TEXT_REGION_LENGTH__=28K)

  # generate firmware .hex file
  objcopy(firmware "ihex" ".hex")

  get_dependency_directory(prusa3dboards PRUSA_BOARDS_DIR)

  add_custom_command(
    TARGET firmware
    POST_BUILD
    COMMAND
      ${CMAKE_OBJCOPY} -I ihex -O binary
      ${PRUSA_BOARDS_DIR}/bootloaders/prusa_mm_control/Caterina-prusa_mm_control.hex bootloader.bin
    COMMAND ${CMAKE_OBJCOPY} firmware -O binary --gap-fill 0xFF --pad-to 0x00007000 firmware.bin
    COMMAND ${CMAKE_COMMAND} -E cat firmware.bin bootloader.bin > fw_bootloader.bin
    COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex fw_bootloader.bin
            "MMU2S_MMU3_BOOTLOADER_${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}.hex"
    COMMAND
      ${CMAKE_COMMAND} -E copy
      "MMU2S_MMU3_BOOTLOADER_${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}.hex"
      "MMU2S_MMU3_BOOTLOADER_${PROJECT_VERSION}.hex"
    BYPRODUCTS bootloader.bin firmware.bin fw_bootloader.bin
    )

  # produce ASM listing
  add_custom_command(
    TARGET firmware
    POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} --prefix ${CMAKE_SOURCE_DIR} -CSd firmware > firmware.asm
    BYPRODUCTS firmware.asm
    )

  # inform about the firmware's size in terminal
  add_custom_command(
    TARGET firmware
    POST_BUILD
    COMMAND ${CMAKE_SIZE_UTIL} -C --mcu=atmega32u4 firmware
    )
  report_size(firmware)

  # generate linker map file
  target_link_options(firmware PUBLIC -Wl,-Map=firmware.map)

  # Put Prusa Magic™ at the beginning of the hex
  add_custom_command(
    TARGET firmware
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -D WORK_DIR=${CMAKE_BINARY_DIR} -D
      HEX_NAME="MMU2S_MMU3_FW${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}.hex" -P
      ${CMAKE_SOURCE_DIR}/cmake/HexConcat.cmake DEPENDS firmware.hex
    COMMAND
      ${CMAKE_COMMAND} -E copy "MMU2S_MMU3_FW${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}.hex"
      "MMU2S_MMU3_FW${PROJECT_VERSION}.hex"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/autopublish
    COMMAND ${CMAKE_COMMAND} -E copy
            "MMU2S_MMU3_FW${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX_SHORT}.hex" "autopublish/"
    )

endif()

target_include_directories(firmware PRIVATE src lib)
if(MMU_BOARD STREQUAL "MMU2_DIP")
  target_include_directories(firmware PRIVATE lib/cmsis_stm32f0)
endif()

target_compile_options(firmware PRIVATE -Wdouble-promotion)

add_subdirectory(src)

if(CMAKE_CROSSCOMPILING)
  set_all_targets_reproducible()
else()
  # do not build the firmware by default (tests are the focus if not crosscompiling)
  set_target_properties(firmware PROPERTIES EXCLUDE_FROM_ALL YES)

  enable_testing()
  add_subdirectory(tests)
endif()
