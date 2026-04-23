cmake_minimum_required(VERSION 3.16)

include(pico_sdk_import.cmake)

project(feather_dualsense C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

option(BUILD_DEBUG "Enable debug UART logging" OFF)
set(HOST_VBUS_EN_PIN "-1" CACHE STRING "Optional GPIO pin to enable host VBUS power; -1 disables")

pico_sdk_init()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/build_config.h
               @ONLY)

add_executable(feather_dualsense
  src/main.cc
  src/host_reader.cc
  src/device_out.cc
)

target_include_directories(feather_dualsense PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Keep aggressive optimization flags local to this firmware target only.
target_compile_options(feather_dualsense PRIVATE
  -ffunction-sections
  -fdata-sections
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<CONFIG:Release>:-O3>
)

target_link_options(feather_dualsense PRIVATE
  -Wl,--gc-sections
)

target_compile_definitions(feather_dualsense PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=0
  PICO_DEFAULT_UART_RX_PIN=1
  CFG_TUH_ENABLED=1
  CFG_TUD_ENABLED=1
  HOST_VBUS_EN_PIN=${HOST_VBUS_EN_PIN}
)

target_link_libraries(feather_dualsense PRIVATE
  pico_stdlib
  tinyusb_device
  tinyusb_host
  tinyusb_board
)

pico_enable_stdio_uart(feather_dualsense 1)
pico_enable_stdio_usb(feather_dualsense 0)

pico_add_extra_outputs(feather_dualsense)
