cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_TESTS "Build unit tests" ON)

include(compilerconfig)
include(defaults)
include(helpers)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 COMPONENTS Widgets Core Network REQUIRED)

# Shared core: OBS-independent logic (also linked by unit tests)
add_library(multistream-core STATIC
  src/core/capabilities.cpp
  src/core/redact.cpp
  src/core/backoff.cpp
  src/core/http-client.cpp
  src/core/config-store.cpp
  src/core/credential-store.cpp
  src/core/oauth-flow.cpp
  src/core/oauth-flow.hpp
  src/core/adapter-registry.cpp
  src/platforms/manual-adapter.cpp
  src/platforms/youtube/youtube-adapter.cpp
  src/platforms/twitch/twitch-adapter.cpp
  src/platforms/kick/kick-adapter.cpp
  src/platforms/vimeo/vimeo-adapter.cpp
  src/platforms/facebook/facebook-adapter.cpp
  src/platforms/restream/restream-adapter.cpp
  src/platforms/trovo/trovo-adapter.cpp
)
target_link_libraries(multistream-core PUBLIC Qt6::Core Qt6::Network)
if(WIN32)
  target_link_libraries(multistream-core PUBLIC Advapi32)
elseif(APPLE)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)
  target_link_libraries(multistream-core PUBLIC "${SECURITY_FRAMEWORK}")
endif()
target_include_directories(multistream-core PUBLIC src)
set_target_properties(multistream-core PROPERTIES AUTOMOC ON CXX_VISIBILITY_PRESET hidden)
target_compile_options(multistream-core PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-comma>)

add_library(${CMAKE_PROJECT_NAME} MODULE
  src/plugin-main.cpp
  src/core/stream-controller.hpp
  src/core/multi-output-engine.hpp
  src/ui/multistream-dock.hpp
  src/ui/platform-picker-dialog.hpp
  src/ui/destination-panel.hpp
  src/core/stream-controller.cpp
  src/core/multi-output-engine.cpp
  src/core/obs-utils.cpp
  src/ui/multistream-dock.cpp
  src/ui/platform-picker-dialog.cpp
  src/ui/destination-panel.cpp
  src/ui/badges.cpp
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)

target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE OBS::libobs OBS::obs-frontend-api multistream-core Qt6::Core Qt6::Widgets Qt6::Network
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(WIN32)
  install(FILES LICENSE README.md DESTINATION "${CMAKE_PROJECT_NAME}")
  install(DIRECTORY docs DESTINATION "${CMAKE_PROJECT_NAME}")
endif()

if(ENABLE_TESTS)
  if(WIN32 AND NOT Qt6Test_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/qt-test-sdk/6.8.3/msvc2022_64/lib/cmake/Qt6Test")
    set(Qt6Test_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/qt-test-sdk/6.8.3/msvc2022_64/lib/cmake/Qt6Test")
  endif()
  find_package(Qt6 COMPONENTS Test REQUIRED)
  enable_testing()
  add_subdirectory(tests)
endif()
