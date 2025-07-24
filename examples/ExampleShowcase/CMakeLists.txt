cmake_minimum_required(VERSION 3.21)
project(ExampleShowcase VERSION 0.1.0 LANGUAGES C CXX)

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
file(GLOB_RECURSE EXAMPLE_SHOWCASE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Create plugin using the simplified API
add_applause_plugin(
    TARGET_NAME ${PROJECT_NAME}
    OUTPUT_NAME "ExampleShowcase"
    PLUGIN_CLASS ExampleShowcasePlugin
    PLUGIN_HEADER "ExampleShowcasePlugin.h"
    
    # Plugin descriptor
    PLUGIN_ID "fm.gns.applause.example.showcase"
    PLUGIN_NAME "Example Showcase"
    PLUGIN_VENDOR "Showcase Framework"
    PLUGIN_URL "example.com"
    PLUGIN_VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "Example plugin showcasing the Applause framework with CMake API"
    # Search for plugin-features.h in the CLAP source repository for a complete list of CLAP feature flags.
    PLUGIN_FEATURES INSTRUMENT SYNTHESIZER
    
    SOURCES ${EXAMPLE_SHOWCASE_SOURCES}
    
    # Plugin formats to generate
    PLUGIN_FORMATS CLAP VST3 AUV2 Standalone
    
    # Bundle information
    BUNDLE_IDENTIFIER "fm.gns.applause.example.showcase"
    BUNDLE_VERSION ${PROJECT_VERSION}
    
    # AUV2 specific
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "ExSh"
    AUV2_INSTRUMENT_TYPE "aumu"
    
    # Options
    COPY_AFTER_BUILD TRUE
)