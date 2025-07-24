cmake_minimum_required(VERSION 3.21)
project(ApplauseExample VERSION 0.1.0 LANGUAGES C CXX)

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
set(APPLAUSE_EXAMPLE_SOURCES
        src/ApplauseExamplePlugin.cpp
        src/ApplauseExamplePlugin.h
        src/ApplauseExampleEditor.cpp
        src/ApplauseExampleEditor.h
)

add_library(${PROJECT_NAME}-impl STATIC ${APPLAUSE_EXAMPLE_SOURCES})

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
    OUTPUT_NAME "ApplauseExample"
    ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/ApplauseExamplePluginEntry.cpp"
    BUNDLE_IDENTIFIER "com.applause.example"
    BUNDLE_VERSION ${PROJECT_VERSION}
    COPY_AFTER_BUILD TRUE
    PLUGIN_FORMATS CLAP VST3 AUV2
    
    # AUV2 metadata
    AUV2_MANUFACTURER_NAME "APP"
    AUV2_MANUFACTURER_CODE "App1"
    AUV2_SUBTYPE_CODE "ApEx"
    AUV2_INSTRUMENT_TYPE "aumu"
)

# On macOS re-sign the VST3 bundle
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME}_vst3 POST_BUILD
        COMMAND codesign --force --sign - --timestamp=none
        "$<TARGET_FILE_DIR:${PROJECT_NAME}_vst3>/../.."
        COMMENT "Ad-hoc signing build directory VST3 bundle"
    )
endif()

# Add this example to the parent applause-examples target if it exists
if(TARGET applause-examples)
    add_dependencies(applause-examples ${PROJECT_NAME}_all)
endif()