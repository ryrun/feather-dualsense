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

# Pico-PIO-USB pio_usb.c has a sign-compare issue (crc_receive ^ 0xffff compared
# unsigned) that GCC flags as an error. Suppress it for that one third-party file.
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB/src/pio_usb.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-sign-compare"
)

option(FEATHER_REMAPPER_DEBUG "Enable verbose UART report logging" OFF)
option(FEATHER_REMAPPER_ENABLE_LTO "Enable link-time optimization in Release builds" OFF)
option(FEATHER_GYRO_STICK_PROFILE "Enable the optional gyro-to-right-stick runtime profile" OFF)

function(add_feather_remapper_target target_name backend_dualshock4)
  add_executable(${target_name}
    src/main.cc
    src/mode.cc
    src/host_reader.cc
    src/device_out.cc
    src/usb_descriptors.cc
  )

  pico_enable_stdio_usb(${target_name} 0)
  pico_enable_stdio_uart(${target_name} 1)

  target_include_directories(${target_name} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
  )

  target_compile_definitions(${target_name} PRIVATE
    PICO_STDIO_USB=0
    PICO_STDIO_UART=1
    CFG_TUSB_MCU=OPT_MCU_RP2040
    PIO_USB_USE_TINYUSB
    BOARD_TUD_RHPORT=0
    BOARD_TUH_RHPORT=1
    FEATHER_REMAPPER_DEBUG=$<BOOL:${FEATHER_REMAPPER_DEBUG}>
    FEATHER_GAMEPAD_BACKEND_DUALSHOCK4=${backend_dualshock4}
    GYRO_STICK_PROFILE_ENABLE=$<BOOL:${FEATHER_GYRO_STICK_PROFILE}>
  )

  target_compile_options(${target_name} PRIVATE
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

  target_link_options(${target_name} PRIVATE
    -Wl,--gc-sections
    $<$<AND:$<CONFIG:Release>,$<BOOL:${FEATHER_REMAPPER_ENABLE_LTO}>>:-flto>
  )

  target_link_libraries(${target_name}
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

  pico_add_extra_outputs(${target_name})
endfunction()

add_feather_remapper_target(feather_remapper_stadia 0)
add_feather_remapper_target(feather_remapper_ds4 1)
