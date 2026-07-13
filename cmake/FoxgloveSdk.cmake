include(FetchContent)

set(HAKO_FOXGLOVE_SDK_VERSION "0.25.2" CACHE STRING "Pinned Foxglove SDK release version")

if(HAKO_PDU_FOXGLOVE_USE_SYSTEM_FOXGLOVE_SDK)
  find_package(foxglove-sdk CONFIG REQUIRED)
else()
  if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
      set(_hako_foxglove_target "aarch64-apple-darwin")
      set(_hako_foxglove_sha "8839bde7c4e1142e7cbbf57756d87d2e36a683fad6fb32cbc240038d6e781ad8")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
      set(_hako_foxglove_target "x86_64-apple-darwin")
      set(_hako_foxglove_sha "3ef620496dde842bc35201f87c33cf94e3b3b2b5b5fb8f057ad64bcf0b12238b")
    else()
      message(FATAL_ERROR "Unsupported macOS processor for Foxglove SDK: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
  elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      set(_hako_foxglove_target "aarch64-unknown-linux-gnu")
      set(_hako_foxglove_sha "72d403cc2ee90e84bd803c08e7dcaa2ddf5175d381f9668b595357a9445c39b6")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
      set(_hako_foxglove_target "x86_64-unknown-linux-gnu")
      set(_hako_foxglove_sha "ac001974aa0e3ac5b8159bcd698007c0661715c4a0201c99353e8a8dfe03bd44")
    else()
      message(FATAL_ERROR "Unsupported Linux processor for Foxglove SDK: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
  else()
    message(FATAL_ERROR "Foxglove SDK archive selection is implemented for macOS and Linux")
  endif()

  set(_hako_foxglove_archive "foxglove-v${HAKO_FOXGLOVE_SDK_VERSION}-cpp-${_hako_foxglove_target}.zip")
  FetchContent_Declare(
    foxglove_sdk_dist
    URL "https://github.com/foxglove/foxglove-sdk/releases/download/sdk/v${HAKO_FOXGLOVE_SDK_VERSION}/${_hako_foxglove_archive}"
    URL_HASH "SHA256=${_hako_foxglove_sha}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(foxglove_sdk_dist)
  find_package(foxglove-sdk CONFIG REQUIRED HINTS "${foxglove_sdk_dist_SOURCE_DIR}")
endif()

foxglove_sdk_add_cpp_library(foxglove_cpp TYPE STATIC REMOTE_ACCESS OFF)
if(APPLE)
  target_link_libraries(foxglove_cpp PUBLIC "-framework IOKit")
endif()
