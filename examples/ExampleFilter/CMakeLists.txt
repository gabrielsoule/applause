cmake_minimum_required(VERSION 3.21)
project(ExampleFilter VERSION 0.1.0 LANGUAGES C CXX)

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

# Include CPM for dependency management
include(../../cmake/CPM.cmake)

# Add chowdsp_utils for DSP processing
CPMAddPackage(
    NAME chowdsp_utils
    GITHUB_REPOSITORY Chowdhury-DSP/chowdsp_utils
    GIT_TAG v2.2.0  # Using a stable version tag
    OPTIONS 
        "BUILD_JUCE_MODULES OFF"  # We're not using JUCE
)

# Create static library from chowdsp modules for filter processing
if(chowdsp_utils_ADDED)
    setup_chowdsp_lib(chowdsp_filter_lib
        MODULES 
            chowdsp_math        # Math utilities
            chowdsp_dsp_utils   # General DSP utilities
            chowdsp_filters     # Filter implementations
    )
endif()

# Check if we're being built as part of the parent project
if(NOT TARGET Applause::Applause)
    # Include parent Applause library only if not already included
    add_subdirectory(../.. applause_parent EXCLUDE_FROM_ALL)
endif()

# Auto-discover source files
file(GLOB_RECURSE EXAMPLE_FILTER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Create plugin using the simplified API
add_applause_plugin(
    TARGET_NAME ${PROJECT_NAME}
    OUTPUT_NAME "ExampleFilter"
    PLUGIN_CLASS ExampleFilterPlugin
    PLUGIN_HEADER "ExampleFilterPlugin.h"
    
    # Plugin descriptor
    PLUGIN_ID "fm.gns.applause.example.filter"
    PLUGIN_NAME "Example Filter"
    PLUGIN_VENDOR "Applause Framework"
    PLUGIN_URL "example.com"
    PLUGIN_VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "Example filter plugin showcasing audio effect processing"
    # Configure as audio effect with filter feature
    PLUGIN_FEATURES AUDIO_EFFECT FILTER STEREO
    
    SOURCES ${EXAMPLE_FILTER_SOURCES}
    
    # Plugin formats to generate
    PLUGIN_FORMATS CLAP VST3 AUV2 Standalone
    
    # Bundle information
    BUNDLE_IDENTIFIER "fm.gns.applause.example.filter"
    BUNDLE_VERSION ${PROJECT_VERSION}
    
    # AUV2 specific - configured as audio effect (aufx)
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "ExFl"
    AUV2_INSTRUMENT_TYPE "aufx"  # Audio effect type for AU
    
    # Options
    COPY_AFTER_BUILD TRUE
)

# Link chowdsp library to the plugin implementation AND all format targets
if(chowdsp_utils_ADDED)
    target_link_libraries(${PROJECT_NAME}-impl PRIVATE chowdsp_filter_lib)
    
    # Also link to all the format targets so they get the PUBLIC includes
    # The formats are: CLAP VST3 AUV2 Standalone
    if(TARGET ${PROJECT_NAME}_clap)
        target_link_libraries(${PROJECT_NAME}_clap PRIVATE chowdsp_filter_lib)
    endif()
    if(TARGET ${PROJECT_NAME}_vst3)
        target_link_libraries(${PROJECT_NAME}_vst3 PRIVATE chowdsp_filter_lib)
    endif()
    if(TARGET ${PROJECT_NAME}_auv2)
        target_link_libraries(${PROJECT_NAME}_auv2 PRIVATE chowdsp_filter_lib)
    endif()
    if(TARGET ${PROJECT_NAME}_standalone)
        target_link_libraries(${PROJECT_NAME}_standalone PRIVATE chowdsp_filter_lib)
    endif()
endif()