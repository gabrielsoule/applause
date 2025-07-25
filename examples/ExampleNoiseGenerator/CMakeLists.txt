cmake_minimum_required(VERSION 3.21)
project(ExampleNoiseGenerator VERSION 0.1.0 LANGUAGES C CXX)

# Enable OBJC/OBJCXX on macOS
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Check if we're being built as part of the parent project
if(NOT TARGET Applause::Applause)
    # Include parent Applause library only if not already included
    add_subdirectory(../.. applause_parent EXCLUDE_FROM_ALL)
endif()

# Auto-discover source files
file(GLOB_RECURSE EXAMPLE_NOISE_GENERATOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Create plugin using the simplified API
add_applause_plugin(
    TARGET_NAME ${PROJECT_NAME}
    OUTPUT_NAME "ExampleNoiseGenerator"
    PLUGIN_CLASS ExampleNoiseGeneratorPlugin
    PLUGIN_HEADER "ExampleNoiseGeneratorPlugin.h"
    
    # Plugin descriptor
    PLUGIN_ID "fm.gns.applause.example.noisegenerator"
    PLUGIN_NAME "Example Noise Generator"
    PLUGIN_VENDOR "Applause Framework"
    PLUGIN_URL "example.com"
    PLUGIN_VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "Simple white noise generator demonstrating the Applause framework"
    PLUGIN_FEATURES INSTRUMENT SYNTHESIZER STEREO
    
    SOURCES ${EXAMPLE_NOISE_GENERATOR_SOURCES}
    
    # Plugin formats to generate
    PLUGIN_FORMATS CLAP VST3 AUV2
    
    # Bundle information
    BUNDLE_IDENTIFIER "fm.gns.applause.example.noisegenerator"
    BUNDLE_VERSION ${PROJECT_VERSION}
    
    # AUV2 specific
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "ENSG"
    AUV2_INSTRUMENT_TYPE "aumu"
    
    # Options
    COPY_AFTER_BUILD TRUE
)