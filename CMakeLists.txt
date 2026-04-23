cmake_minimum_required(VERSION 3.16)

include(pico_sdk_import.cmake)

project(feather_dualsense C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

set(COMMON_FLAGS "-ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -flto")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto")
endif ()

option(BUILD_DEBUG "Enable debug UART logging" OFF)

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

target_compile_definitions(feather_dualsense PRIVATE
  PICO_DEFAULT_UART_TX_PIN=0
  PICO_DEFAULT_UART_RX_PIN=1
)

target_link_libraries(feather_dualsense PRIVATE
  pico_stdlib
  tinyusb_device
  tinyusb_host
  tinyusb_board
)

pico_enable_stdio_uart(feather_dualsense ${BUILD_DEBUG})
pico_enable_stdio_usb(feather_dualsense 0)

pico_add_extra_outputs(feather_dualsense)

# Ensure TinyUSB host + device are both enabled.
target_compile_definitions(feather_dualsense PRIVATE
  CFG_TUH_ENABLED=1
  CFG_TUD_ENABLED=1
)
