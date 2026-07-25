cmake_minimum_required(VERSION 3.20)

project(hakoniwa-pdu-foxglove VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HAKO_PDU_FOXGLOVE_BUILD_TESTS "Build tests" ON)
option(HAKO_PDU_FOXGLOVE_BUILD_EXAMPLES "Build examples" ON)
option(HAKO_PDU_FOXGLOVE_FETCH_DEPS "Fetch missing pinned dependencies" ON)
option(HAKO_PDU_FOXGLOVE_USE_SYSTEM_FOXGLOVE_SDK "Use an already-installed foxglove-sdk package" OFF)

set(HAKO_PDU_ENDPOINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HAKO_PDU_ENDPOINT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(HAKO_PDU_ENDPOINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(HAKO_PDU_ENDPOINT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HAKO_PDU_ENDPOINT_INSTALL OFF CACHE BOOL "" FORCE)
set(HAKO_PDU_ENDPOINT_ENABLE_HAKONIWA_CORE OFF CACHE BOOL "" FORCE)
add_subdirectory(hakoniwa-pdu-endpoint)

include(cmake/FoxgloveSdk.cmake)

add_library(hakoniwa_pdu_foxglove
  src/foxglove_config.cpp
  src/foxglove_publisher.cpp
  src/comm_foxglove.cpp
)
add_library(hakoniwa_pdu_foxglove::hakoniwa_pdu_foxglove ALIAS hakoniwa_pdu_foxglove)

if(MSVC)
  # Foxglove SDK v0.25.2's parameter.hpp has a template-specialization ordering
  # pattern that MSVC rejects under strict two-phase lookup. Keep this workaround
  # scoped to this consumer target until the pinned/upstream SDK no longer needs it.
  target_compile_options(hakoniwa_pdu_foxglove PRIVATE /Zc:twoPhase-)
endif()

target_include_directories(hakoniwa_pdu_foxglove
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)
target_link_libraries(hakoniwa_pdu_foxglove
  PUBLIC
    hakoniwa_pdu_endpoint
  PRIVATE
    foxglove_cpp
)

if(HAKO_PDU_FOXGLOVE_BUILD_EXAMPLES)
  find_package(fastcdr CONFIG QUIET)
  if(NOT fastcdr_FOUND)
    find_path(FASTCDR_INCLUDE_DIR fastcdr/Cdr.h)
    find_library(FASTCDR_LIBRARY NAMES fastcdr)
    if(NOT FASTCDR_INCLUDE_DIR OR NOT FASTCDR_LIBRARY)
      message(FATAL_ERROR "Fast CDR was not found. Install fastcdr or set CMAKE_PREFIX_PATH to its CMake package.")
    endif()
    add_library(fastcdr_fallback UNKNOWN IMPORTED)
    set_target_properties(fastcdr_fallback PROPERTIES
      IMPORTED_LOCATION "${FASTCDR_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${FASTCDR_INCLUDE_DIR}"
    )
    set(HAKO_FASTCDR_TARGET fastcdr_fallback)
  elseif(TARGET fastcdr)
    set(HAKO_FASTCDR_TARGET fastcdr)
  elseif(TARGET fastcdr::fastcdr)
    set(HAKO_FASTCDR_TARGET fastcdr::fastcdr)
  else()
    message(FATAL_ERROR "fastcdr package found, but no known target exists")
  endif()

  add_executable(cdr_publisher_example examples/cdr-publisher/main.cpp)
  target_include_directories(cdr_publisher_example
    PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/hakoniwa-pdu-registry/pdu/types"
  )
  target_link_libraries(cdr_publisher_example
    PRIVATE
      hakoniwa_pdu_foxglove
      ${HAKO_FASTCDR_TARGET}
  )

  add_executable(cdr_stdin_publisher examples/cdr-stdin-publisher/main.cpp)
  target_link_libraries(cdr_stdin_publisher
    PRIVATE
      hakoniwa_pdu_foxglove
      ${HAKO_FASTCDR_TARGET}
  )
endif()

include(CTest)
if(HAKO_PDU_FOXGLOVE_BUILD_TESTS AND BUILD_TESTING)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND AND HAKO_PDU_FOXGLOVE_FETCH_DEPS)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(hakoniwa_pdu_foxglove_tests
    test/test_config.cpp
    test/test_comm_foxglove.cpp
    test/test_publisher.cpp
  )
  target_link_libraries(hakoniwa_pdu_foxglove_tests
    PRIVATE
      hakoniwa_pdu_foxglove
      GTest::gtest_main
  )
  add_test(NAME hakoniwa_pdu_foxglove_tests COMMAND hakoniwa_pdu_foxglove_tests)
  set_tests_properties(hakoniwa_pdu_foxglove_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif()
