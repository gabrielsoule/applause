cmake_minimum_required(VERSION 3.21)
project(ExampleManualPluginEntry VERSION 0.1.0 LANGUAGES C CXX)

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

# Create plugin implementation library
set(EXAMPLE_MANUAL_PLUGIN_ENTRY_SOURCES
        src/ExampleManualPluginEntryPlugin.cpp
        src/ExampleManualPluginEntryPlugin.h
        src/ExampleManualPluginEntryEditor.cpp
        src/ExampleManualPluginEntryEditor.h
)

add_library(${PROJECT_NAME}-impl STATIC ${EXAMPLE_MANUAL_PLUGIN_ENTRY_SOURCES})

# Link with Applause and its dependencies
target_link_libraries(${PROJECT_NAME}-impl PUBLIC
    Applause::Applause
)

# Include directories
target_include_directories(${PROJECT_NAME}-impl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform specific
if(WIN32)
    target_compile_definitions(${PROJECT_NAME}-impl PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# Create plugin using clap-wrapper
make_clapfirst_plugins(
    TARGET_NAME ${PROJECT_NAME}
    IMPL_TARGET ${PROJECT_NAME}-impl
    OUTPUT_NAME "ExampleManualPluginEntry"
    ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/ExampleManualPluginEntryPluginEntry.cpp"
    BUNDLE_IDENTIFIER "fm.gns.applause.examples.manualpluginentry"
    BUNDLE_VERSION ${PROJECT_VERSION}
    COPY_AFTER_BUILD TRUE
    PLUGIN_FORMATS CLAP VST3 AUV2
    
    # AUV2 metadata
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "EMPE"
    AUV2_INSTRUMENT_TYPE "aumu"
)

# On macOS re-sign the VST3 bundle AFTER installation
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME}_vst3 POST_BUILD
        COMMAND codesign --force --deep --sign - --timestamp=none
        "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/$<TARGET_PROPERTY:${PROJECT_NAME}_vst3,LIBRARY_OUTPUT_NAME>.vst3"
        COMMENT "Ad-hoc signing installed VST3 bundle"
    )
endif()

# Add this example to the parent applause-examples target if it exists
if(TARGET applause-examples)
    add_dependencies(applause-examples ${PROJECT_NAME}_all)
endif()