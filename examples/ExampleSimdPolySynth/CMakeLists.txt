cmake_minimum_required(VERSION 3.21)
project(ExampleSimdPolySynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT TARGET Applause::Applause)
    add_subdirectory(../.. applause_parent EXCLUDE_FROM_ALL)
endif ()

if(APPLAUSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
