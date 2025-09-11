cmake_minimum_required(VERSION 3.21)
project(ExampleMIDIPrinter VERSION 0.1.0 LANGUAGES C CXX)

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
file(GLOB_RECURSE EXAMPLE_MIDI_PRINTER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Create plugin using the simplified API
add_applause_plugin(
    TARGET_NAME ${PROJECT_NAME}
    OUTPUT_NAME "ExampleMIDIPrinter"
    PLUGIN_CLASS ExampleMIDIPrinterPlugin
    PLUGIN_HEADER "ExampleMIDIPrinterPlugin.h"
    
    # Plugin descriptor
    PLUGIN_ID "fm.gns.applause.example.midiprinter"
    PLUGIN_NAME "Example MIDI Printer"
    PLUGIN_VENDOR "Applause Framework"
    PLUGIN_URL "example.com"
    PLUGIN_VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "MIDI event printer for analyzing different note input formats (MIDI, MPE, MIDI2, CLAP)"
    PLUGIN_FEATURES INSTRUMENT SYNTHESIZER
    
    SOURCES ${EXAMPLE_MIDI_PRINTER_SOURCES}
    
    # Plugin formats to generate
    PLUGIN_FORMATS CLAP VST3 AUV2
    
    # Bundle information
    BUNDLE_IDENTIFIER "fm.gns.applause.example.midiprinter"
    BUNDLE_VERSION ${PROJECT_VERSION}
    
    # AUV2 specific
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "EMPR"
    AUV2_INSTRUMENT_TYPE "aumf"
    
    # Options
    COPY_AFTER_BUILD TRUE
)