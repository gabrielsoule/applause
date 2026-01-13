cmake_minimum_required(VERSION 3.21)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS deployment target" FORCE)
    message(STATUS "Applause: Setting CMAKE_OSX_DEPLOYMENT_TARGET to ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

project(Applause VERSION 0.1.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(cmake/CPM.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplausePlugin.cmake)


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

# nlohmann/json for state serialization
CPMAddPackage(
    NAME nlohmann_json
    VERSION 3.11.3
    GITHUB_REPOSITORY nlohmann/json
    OPTIONS "JSON_BuildTests OFF"
)

# xsimd (header-only SIMD library)
CPMAddPackage(
    NAME xsimd
    GITHUB_REPOSITORY xtensor-stack/xsimd
    GIT_TAG 13.0.0
    OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_BENCHMARK OFF"
)

# Visage, now with commit pinning since Visage still occasionally pushes breaking changes...
CPMAddPackage(
    NAME visage
    GITHUB_REPOSITORY vitalaudio/visage
    GIT_TAG 54b11a1
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

# Native File Dialog Extended
CPMAddPackage(
    NAME nativefiledialog_extended
    GITHUB_REPOSITORY btzy/nativefiledialog-extended
    GIT_TAG v1.2.1
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "NFD_BUILD_TESTS OFF"
        "NFD_BUILD_SDL2_TESTS OFF"
        "NFD_INSTALL OFF"
)

# Embed font resources
file(GLOB APPLAUSE_FONT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/*.ttf)
add_embedded_resources(ApplauseEmbeddedFonts "applause_fonts.h" "applause::fonts" "${APPLAUSE_FONT_FILES}")

file(GLOB_RECURSE APPLAUSE_SOURCES applause/*.cpp applause/*.h)
if(APPLE)
    file(GLOB_RECURSE APPLAUSE_OBJCXX_SOURCES applause/*.mm)
    list(APPEND APPLAUSE_SOURCES ${APPLAUSE_OBJCXX_SOURCES})
endif()

add_library(applause STATIC ${APPLAUSE_SOURCES})
add_library(Applause::Applause ALIAS applause)

target_include_directories(applause PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(applause
    PUBLIC
        clap
        clap-helpers
        visage
        nfd
        VisageEmbeddedFonts
        ApplauseEmbeddedFonts
        nlohmann_json::nlohmann_json
        xsimd
)

if(APPLAUSE_USE_FMT)
    message(STATUS "Configuring Applause with fmt")
    CPMAddPackage(
        NAME fmt
        GITHUB_REPOSITORY fmtlib/fmt
        GIT_TAG 11.2.0
    )
    target_link_libraries(applause PUBLIC fmt)
    target_compile_definitions(applause PUBLIC APPLAUSE_USE_FMT)
endif()

# Platform-specific
if(WIN32)
    target_compile_definitions(applause PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(APPLE)
    target_link_libraries(applause PRIVATE "-framework CoreFoundation")
endif()

# Examples
option(APPLAUSE_BUILD_EXAMPLES "Build example plugins" ${PROJECT_IS_TOP_LEVEL})
if(APPLAUSE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(APPLAUSE_BUILD_TESTS "Build unit tests" OFF)
if(APPLAUSE_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Applause configured successfully")
