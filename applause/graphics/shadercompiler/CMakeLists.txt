# The build-time shader compiler. It's also its own project, so it can build for the host when cross-compiling.
cmake_minimum_required(VERSION 3.21)
project(ApplauseShaderCompiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${config} config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_CURRENT_BINARY_DIR}/bin)
endforeach()

if(NOT COMMAND CPMAddPackage)
    include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/CPM.cmake)
endif()

set(CMAKE_MESSAGE_LOG_LEVEL ERROR)
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
    NAME glslang
    GITHUB_REPOSITORY KhronosGroup/glslang
    GIT_TAG vulkan-sdk-1.4.341.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "ENABLE_OPT OFF"
        "ENABLE_GLSLANG_BINARIES OFF"
        "ENABLE_HLSL OFF"
        "ENABLE_SPVREMAPPER OFF"
        "ENABLE_CTEST OFF"
        "GLSLANG_TESTS OFF"
        "GLSLANG_ENABLE_INSTALL OFF"
)
CPMAddPackage(
    NAME spirv_cross
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
    GIT_TAG vulkan-sdk-1.4.341.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SPIRV_CROSS_CLI OFF"
        "SPIRV_CROSS_ENABLE_TESTS OFF"
        "SPIRV_CROSS_ENABLE_C_API OFF"
        "SPIRV_CROSS_ENABLE_UTIL OFF"
        "SPIRV_CROSS_ENABLE_REFLECT OFF"
        "SPIRV_CROSS_ENABLE_CPP OFF"
        "SPIRV_CROSS_SKIP_INSTALL ON"
)
unset(CMAKE_MESSAGE_LOG_LEVEL)

set(shader_compiler_sources ShaderCompiler.cpp ShaderCompiler.h ../gpu/GpuShader.cpp)
if(WIN32)
    list(APPEND shader_compiler_sources ShaderCompilerDxbc.cpp)
elseif(APPLE)
    list(APPEND shader_compiler_sources ShaderCompilerMetallib.cpp)
endif()

add_library(ApplauseShaderCompiler STATIC ${shader_compiler_sources})
target_include_directories(ApplauseShaderCompiler PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../..)
target_link_libraries(ApplauseShaderCompiler PRIVATE glslang glslang-default-resource-limits SPIRV spirv-cross-core
                      spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
target_compile_features(ApplauseShaderCompiler PUBLIC cxx_std_17)
if(WIN32)
    target_link_libraries(ApplauseShaderCompiler PRIVATE d3dcompiler)
endif()

add_executable(applause_shader_compiler ShaderCompilerMain.cpp)
target_link_libraries(applause_shader_compiler PRIVATE ApplauseShaderCompiler)
