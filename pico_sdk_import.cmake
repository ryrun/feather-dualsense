# Pico SDK importer with fallback fetch support.
# Priority:
# 1) PICO_SDK_PATH cache/environment
# 2) ./pico-sdk submodule
# 3) Git clone into build cache (if PICO_SDK_FETCH_FROM_GIT is ON)

if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif ()

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk" CACHE PATH "Path to the Pico SDK")
option(PICO_SDK_FETCH_FROM_GIT "Fetch pico-sdk if local path is missing" ON)
set(PICO_SDK_FETCH_FROM_GIT_TAG "master" CACHE STRING "Git tag/branch/commit for pico-sdk")

if (EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  return()
endif ()

if (NOT PICO_SDK_FETCH_FROM_GIT)
  message(FATAL_ERROR
    "Pico SDK not found. Clone with submodules or set PICO_SDK_PATH.\n"
    "  git submodule update --init --recursive")
endif ()

include(FetchContent)

set(PICO_SDK_FETCH_DIR "${CMAKE_BINARY_DIR}/_deps/pico-sdk-src")
if (NOT EXISTS "${PICO_SDK_FETCH_DIR}/external/pico_sdk_import.cmake")
  message(STATUS "Fetching pico-sdk (${PICO_SDK_FETCH_FROM_GIT_TAG})")
  FetchContent_Declare(
    pico_sdk_fetch
    GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
    GIT_TAG ${PICO_SDK_FETCH_FROM_GIT_TAG}
    SOURCE_DIR "${PICO_SDK_FETCH_DIR}"
  )
  FetchContent_Populate(pico_sdk_fetch)
endif ()

set(PICO_SDK_PATH "${PICO_SDK_FETCH_DIR}" CACHE PATH "Path to the Pico SDK" FORCE)
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
