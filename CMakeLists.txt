cmake_minimum_required(VERSION 3.24)
project(WojcimierzEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WZ_BUILD_TESTS "Buduj testy jednostkowe (GoogleTest)" ON)
option(WZ_BUILD_SANDBOX "Buduj aplikacje sandbox" ON)
option(WZ_BUILD_RENDER_TESTS "Buduj render_tests (zrzuty scen testowych)" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(cmake/Dependencies.cmake)
add_subdirectory(third_party/glad)
add_subdirectory(engine)

if(WZ_BUILD_SANDBOX)
  add_subdirectory(sandbox)
endif()

if(WZ_BUILD_TESTS OR WZ_BUILD_RENDER_TESTS)
  enable_testing()
endif()

if(WZ_BUILD_RENDER_TESTS)
  add_subdirectory(render_tests)
endif()

if(WZ_BUILD_TESTS)
  add_subdirectory(tests)
endif()
