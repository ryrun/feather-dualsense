# This is a local copy of the Raspberry Pi Pico SDK import helper.
# It supports either:
# - PICO_SDK_PATH set in the environment/cache, or
# - a checked-out pico-sdk directory in this repository.

if(DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")

if(NOT PICO_SDK_PATH)
  set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
endif()

if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
  message(FATAL_ERROR
    "Pico SDK not found. Set PICO_SDK_PATH or check out the pico-sdk submodule at ${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
endif()

include("${PICO_SDK_PATH}/pico_sdk_init.cmake")

