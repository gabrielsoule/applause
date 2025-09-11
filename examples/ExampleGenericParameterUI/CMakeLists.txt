cmake_minimum_required(VERSION 3.21)
project(ExampleGenericParameterUI VERSION 0.1.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(../../cmake/CPM.cmake)

# Check if we're being built as part of the parent project
if(NOT TARGET Applause::Applause)
    # Include parent Applause library only if not already included
    add_subdirectory(../.. applause_parent EXCLUDE_FROM_ALL)
endif()

file(GLOB_RECURSE EXAMPLE_GENERICPARAMETERUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_applause_plugin(
    TARGET_NAME ${PROJECT_NAME}
    OUTPUT_NAME "ExampleGenericParameterUI"
    PLUGIN_CLASS ExampleGenericParameterUIPlugin
    PLUGIN_HEADER "ExampleGenericParameterUIPlugin.h"
    
    # Plugin descriptor
    PLUGIN_ID "fm.gns.applause.example.genericparameterui"
    PLUGIN_NAME "Example Generic Parameter UI"
    PLUGIN_VENDOR "Applause Framework"
    PLUGIN_URL "example.com"
    PLUGIN_VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "Example plugin showcasing the GenericParameterUIEditor"
    PLUGIN_FEATURES AUDIO_EFFECT
    
    SOURCES ${EXAMPLE_GENERICPARAMETERUI_SOURCES}
    
    # Plugin formats to generate
    PLUGIN_FORMATS CLAP VST3 AUV2 Standalone
    
    # Bundle information
    BUNDLE_IDENTIFIER "fm.gns.applause.example.genericparameterui"
    BUNDLE_VERSION ${PROJECT_VERSION}
    
    # AUV2 specific - configured as audio effect (aufx)
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "ExFl"
    AUV2_INSTRUMENT_TYPE "aufx"  # Audio effect type for AU
    
    COPY_AFTER_BUILD TRUE
)
