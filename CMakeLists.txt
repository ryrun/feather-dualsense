cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED PICO_BOARD)
  set(PICO_BOARD feather_host CACHE STRING "Target board")
endif()

list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

include(pico_sdk_import.cmake)

project(feather_dualsense C CXX ASM)

pico_sdk_init()

option(FEATHER_REMAPPER_DEBUG "Enable verbose UART report logging" OFF)
option(FEATHER_REMAPPER_ENABLE_LTO "Enable link-time optimization in Release builds" ON)

add_executable(feather_remapper
  src/main.cc
  src/mode.cc
  src/host_reader.cc
  src/device_out.cc
  src/usb_descriptors.cc
)

pico_enable_stdio_usb(feather_remapper 0)
pico_enable_stdio_uart(feather_remapper 1)

target_include_directories(feather_remapper PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_definitions(feather_remapper PRIVATE
  PICO_STDIO_USB=0
  PICO_STDIO_UART=1
  CFG_TUSB_MCU=OPT_MCU_RP2040
  PIO_USB_USE_TINYUSB
  BOARD_TUD_RHPORT=0
  BOARD_TUH_RHPORT=1
  FEATHER_REMAPPER_DEBUG=$<BOOL:${FEATHER_REMAPPER_DEBUG}>
)

target_compile_options(feather_remapper PRIVATE
  -Wall
  -Wextra
  -Werror
  -ffunction-sections
  -fdata-sections
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<CONFIG:Release>:-O3>
  $<$<AND:$<CONFIG:Release>,$<BOOL:${FEATHER_REMAPPER_ENABLE_LTO}>>:-flto>
)

target_link_options(feather_remapper PRIVATE
  -Wl,--gc-sections
  $<$<AND:$<CONFIG:Release>,$<BOOL:${FEATHER_REMAPPER_ENABLE_LTO}>>:-flto>
)

target_link_libraries(feather_remapper
  pico_stdlib
  pico_unique_id
  tinyusb_pico_pio_usb
  tinyusb_device
  tinyusb_host
  tinyusb_board
  hardware_flash
  hardware_watchdog
  hardware_sync
)

pico_add_extra_outputs(feather_remapper)
