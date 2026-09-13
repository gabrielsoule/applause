cmake_minimum_required(VERSION 3.21)
project(ExampleSimdPolySynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT TARGET Applause::Applause)
    add_subdirectory(../.. applause_parent EXCLUDE_FROM_ALL)
endif ()

add_applause_plugin(
        TARGET_NAME ${PROJECT_NAME}
        PLUGIN_CLASS ExampleSimdPolySynthPlugin
        PLUGIN_HEADER "ExampleSimdPolySynthPlugin.h"

        PLUGIN_ID "fm.gns.applause.example.simdpolysynth"
        PLUGIN_NAME "Example SIMD Poly Synth"
        PLUGIN_VENDOR "Applause Framework"
        PLUGIN_VERSION ${PROJECT_VERSION}
        PLUGIN_DESCRIPTION "SIMD-across-voices polyphonic synthesizer demonstration"
        PLUGIN_FEATURES INSTRUMENT SYNTHESIZER STEREO

        SOURCES
        src/ExampleSimdPolySynthEditor.cpp
        src/ExampleSimdPolySynthEditor.h
        src/ExampleSimdPolySynthPlugin.cpp
        src/ExampleSimdPolySynthPlugin.h
        src/BatchedVoice.h
        src/SimdAdsr.h
        src/SimdOscillator.h
        src/SimdPolySynth.h
        src/SimdVoiceManager.h
        src/SimdVoiceProxy.h

        PLUGIN_FORMATS CLAP
        COPY_AFTER_BUILD TRUE
)

if(APPLAUSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
