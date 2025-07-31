cmake_minimum_required(VERSION 3.21)

# Set macOS deployment target before project() to ensure it's available everywhere
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS deployment target" FORCE)
    message(STATUS "Applause: Setting CMAKE_OSX_DEPLOYMENT_TARGET to ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

project(Applause VERSION 0.1.0 LANGUAGES C CXX)

# Enable OBJC/OBJCXX on macOS
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include CPM for dependency management
include(cmake/CPM.cmake)

# Include Applause plugin helper functions
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplausePlugin.cmake)

# === Dependencies ===

# CLAP
CPMAddPackage(
    NAME clap
    GITHUB_REPOSITORY free-audio/clap
    GIT_TAG main
    OPTIONS "CLAP_BUILD_TESTS OFF"
)

# CLAP Helpers
CPMAddPackage(
    NAME clap-helpers
    GITHUB_REPOSITORY free-audio/clap-helpers
    GIT_TAG main
    OPTIONS "CLAP_HELPERS_BUILD_TESTS OFF"
)

# YAS (header-only)
CPMAddPackage(
    NAME yas
    GITHUB_REPOSITORY niXman/yas
    GIT_TAG master
    DOWNLOAD_ONLY YES
)
if(yas_ADDED)
    add_library(yas INTERFACE)
    target_include_directories(yas INTERFACE ${yas_SOURCE_DIR}/include)
endif()

# Visage
CPMAddPackage(
    NAME visage
    GITHUB_REPOSITORY vitalaudio/visage
    GIT_TAG main
    OPTIONS
        "VISAGE_BUILD_EXAMPLES OFF"
        "VISAGE_BUILD_TESTS OFF"
)

# CLAP Wrapper
if(APPLE)
    CPMAddPackage(
        NAME clap-wrapper
        GITHUB_REPOSITORY free-audio/clap-wrapper
        GIT_TAG main
        OPTIONS
            "CMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
            "CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE"
            "CLAP_WRAPPER_BUILD_TESTS OFF"
    )
else()
    CPMAddPackage(
        NAME clap-wrapper
        GITHUB_REPOSITORY free-audio/clap-wrapper
        GIT_TAG main
        OPTIONS
            "CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE"
            "CLAP_WRAPPER_BUILD_TESTS OFF"
    )
endif()
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE)

# Include clap-wrapper's CMake functions to make make_clapfirst_plugins available
# The wrapper_functions.cmake uses relative paths, so we need to set the module path
list(APPEND CMAKE_MODULE_PATH "${clap-wrapper_SOURCE_DIR}/cmake")
include(make_clapfirst)

# === Applause Library ===

# Embed font resources
file(GLOB APPLAUSE_FONT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/*.ttf)
add_embedded_resources(ApplauseEmbeddedFonts "applause_fonts.h" "applause::fonts" "${APPLAUSE_FONT_FILES}")

# Collect sources
file(GLOB_RECURSE APPLAUSE_SOURCES src/*.cpp src/*.h)

# Create library
add_library(applause STATIC ${APPLAUSE_SOURCES})
add_library(Applause::Applause ALIAS applause)

# Include directories
target_include_directories(applause PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Link dependencies
target_link_libraries(applause
    PUBLIC
        clap
        clap-helpers
        visage
        VisageEmbeddedFonts
        ApplauseEmbeddedFonts
        yas
)

# Platform-specific
if(WIN32)
    target_compile_definitions(applause PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(APPLE)
    target_link_libraries(applause PRIVATE "-framework CoreFoundation")
endif()

# Examples
option(APPLAUSE_BUILD_EXAMPLES "Build example plugins" ON)
if(APPLAUSE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

message(STATUS "Applause configured successfully")