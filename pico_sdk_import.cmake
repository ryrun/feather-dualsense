# Pico SDK importer with deterministic fallback download support.

if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif ()

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk" CACHE PATH "Path to the Pico SDK")
option(PICO_SDK_FETCH_FROM_GIT "Clone pico-sdk if local path is missing" ON)
set(PICO_SDK_FETCH_FROM_GIT_TAG "master" CACHE STRING "Git tag/branch/commit for pico-sdk")

# Provide custom board definitions for PICO_BOARD=feather_host.
set(PICO_BOARD_CMAKE_DIRS "${CMAKE_CURRENT_LIST_DIR}/boards" CACHE STRING "Extra board cmake dirs" FORCE)
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/boards" CACHE STRING "Extra board header dirs" FORCE)

if (EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  return()
endif ()

if (NOT PICO_SDK_FETCH_FROM_GIT)
  message(FATAL_ERROR
    "Pico SDK not found. Clone with submodules or set PICO_SDK_PATH.\n"
    "  git clone --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git")
endif ()

set(PICO_SDK_FETCH_DIR "${CMAKE_BINARY_DIR}/_deps/pico-sdk-src")
message(STATUS "Cloning pico-sdk (${PICO_SDK_FETCH_FROM_GIT_TAG}) into ${PICO_SDK_FETCH_DIR}")

if (EXISTS "${PICO_SDK_FETCH_DIR}" AND NOT EXISTS "${PICO_SDK_FETCH_DIR}/external/pico_sdk_import.cmake")
  file(REMOVE_RECURSE "${PICO_SDK_FETCH_DIR}")
endif ()

if (NOT EXISTS "${PICO_SDK_FETCH_DIR}/external/pico_sdk_import.cmake")
  execute_process(
    COMMAND git clone --depth 1 --branch ${PICO_SDK_FETCH_FROM_GIT_TAG} --recurse-submodules
            https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_FETCH_DIR}
    RESULT_VARIABLE clone_result
    OUTPUT_VARIABLE clone_stdout
    ERROR_VARIABLE clone_stderr
  )

  if (NOT clone_result EQUAL 0)
    message(FATAL_ERROR
      "Failed to clone pico-sdk from GitHub.\n"
      "stdout:\n${clone_stdout}\n"
      "stderr:\n${clone_stderr}\n"
      "You can also provide PICO_SDK_PATH or initialize pico-sdk as a submodule.")
  endif ()
endif ()

set(PICO_SDK_PATH "${PICO_SDK_FETCH_DIR}" CACHE PATH "Path to the Pico SDK" FORCE)
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
