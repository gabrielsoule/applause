# Graphics and windowing (derived from Visage), with the GPU layer, freetype, the shader compiler and shaders.
# These build as their own targets so their platform code compiles with exactly the flags upstream uses.

option(APPLAUSE_GRAPHICS_DEBUG_LOGGING "Shows graphics debug log in console in debug mode" OFF)
set(APPLAUSE_APPLICATION_NAME ${CMAKE_PROJECT_NAME} CACHE STRING "Default window title and window class name prefix")

set(APPLAUSE_GRAPHICS_DIR ${PROJECT_SOURCE_DIR}/applause/graphics)
set(APPLAUSE_WINDOWING_DIR ${PROJECT_SOURCE_DIR}/applause/windowing)

# Every file that includes the UI headers needs to agree on these.
add_library(ApplausePlatform INTERFACE)
if(WIN32)
    target_compile_definitions(ApplausePlatform INTERFACE APPLAUSE_WINDOWS=1)
elseif(APPLE)
    target_compile_definitions(ApplausePlatform INTERFACE APPLAUSE_MAC=1)
elseif(UNIX)
    target_compile_definitions(ApplausePlatform INTERFACE APPLAUSE_LINUX=1)
endif()

if(MSVC)
    set(APPLAUSE_UI_COMPILE_OPTIONS /MP /wd4244 /wd4267 $<$<CONFIG:Debug>:/JMC>)
else()
    set(APPLAUSE_UI_COMPILE_OPTIONS -Wno-conversion -Wno-sign-conversion)
endif()

# A plugin shares its host's process, so the UI layer keeps its symbols to itself.
set(APPLAUSE_HIDDEN_VISIBILITY C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden OBJC_VISIBILITY_PRESET hidden
    OBJCXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# freetype
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)

set(CMAKE_MESSAGE_LOG_LEVEL ERROR)
CPMAddPackage(
    NAME freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-14-1
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
)
unset(CMAKE_MESSAGE_LOG_LEVEL)

target_compile_options(freetype PRIVATE $<IF:$<BOOL:${MSVC}>,/W0,-w>)
set_target_properties(freetype PROPERTIES ${APPLAUSE_HIDDEN_VISIBILITY})
if(UNIX AND NOT APPLE)
    target_compile_definitions(freetype PRIVATE _LARGEFILE64_SOURCE)
    find_package(Fontconfig REQUIRED)
    set_target_properties(freetype PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
endif()

# The shader compiler runs during the build, so when cross-compiling it builds separately for the host. Set
# APPLAUSE_SHADER_COMPILER to the path of a prebuilt one to skip building it.
set(APPLAUSE_SHADER_COMPILER_DIR ${APPLAUSE_GRAPHICS_DIR}/shadercompiler)
if(APPLAUSE_SHADER_COMPILER)
    set(APPLAUSE_SHADER_COMPILER_EXECUTABLE ${APPLAUSE_SHADER_COMPILER} CACHE INTERNAL "")
    set(APPLAUSE_SHADER_COMPILER_TARGET "" CACHE INTERNAL "")
elseif(NOT CMAKE_CROSSCOMPILING)
    if(NOT TARGET applause_shader_compiler)
        add_subdirectory(${APPLAUSE_SHADER_COMPILER_DIR} ${CMAKE_BINARY_DIR}/applause_shader_compiler)
    endif()
    set(APPLAUSE_SHADER_COMPILER_EXECUTABLE "$<TARGET_FILE:applause_shader_compiler>" CACHE INTERNAL "")
    set(APPLAUSE_SHADER_COMPILER_TARGET applause_shader_compiler CACHE INTERNAL "")
elseif(NOT TARGET ApplauseShaderCompilerHost)
    include(ExternalProject)
    set(shader_compiler_binary_dir ${CMAKE_BINARY_DIR}/applause_shader_compiler_host)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(APPLAUSE_SHADER_COMPILER_EXECUTABLE ${shader_compiler_binary_dir}/bin/applause_shader_compiler.exe
            CACHE INTERNAL "")
    else()
        set(APPLAUSE_SHADER_COMPILER_EXECUTABLE ${shader_compiler_binary_dir}/bin/applause_shader_compiler CACHE INTERNAL "")
    endif()

    set(shader_compiler_args -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=)
    if(CMAKE_CXX_COMPILER_LAUNCHER)
        list(APPEND shader_compiler_args -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
             -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER})
    endif()
    if(CPM_SOURCE_CACHE)
        list(APPEND shader_compiler_args -DCPM_SOURCE_CACHE=${CPM_SOURCE_CACHE})
    endif()

    ExternalProject_Add(ApplauseShaderCompilerHost
        SOURCE_DIR ${APPLAUSE_SHADER_COMPILER_DIR}
        BINARY_DIR ${shader_compiler_binary_dir}
        CMAKE_COMMAND ${CMAKE_COMMAND} -E env --unset=CC --unset=CXX ${CMAKE_COMMAND}
        CMAKE_ARGS ${shader_compiler_args}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target applause_shader_compiler
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${APPLAUSE_SHADER_COMPILER_EXECUTABLE}
        BUILD_ALWAYS TRUE
    )
    set(APPLAUSE_SHADER_COMPILER_TARGET ApplauseShaderCompilerHost CACHE INTERNAL "")
endif()

# Metal shaders compile to a metallib when Xcode's Metal toolchain is installed. Without it they're embedded as source
# and compile at runtime, which stalls the first frame that uses each one.
set(APPLAUSE_METAL_SHADER_TARGET msl CACHE INTERNAL "")
set(APPLAUSE_METAL_SHADER_ARGS "" CACHE INTERNAL "")
if(APPLE)
    set(metal_version_min 10.15)
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(metal_version_min ${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()

    set(metal_check_dir ${CMAKE_BINARY_DIR}/applause_metal_check)
    file(WRITE ${metal_check_dir}/check.metal "kernel void main0() {}\n")
    execute_process(
        COMMAND xcrun -sdk macosx metal -mmacosx-version-min=${metal_version_min} -o ${metal_check_dir}/check.metallib
                ${metal_check_dir}/check.metal
        RESULT_VARIABLE metal_check_result
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(metal_check_result EQUAL 0)
        set(APPLAUSE_METAL_SHADER_TARGET metallib CACHE INTERNAL "")
        set(APPLAUSE_METAL_SHADER_ARGS --macos-version-min ${metal_version_min} CACHE INTERNAL "")
    else()
        message(WARNING "Applause: The Metal Toolchain is missing, so Metal shaders are embedded as source and compile "
                "at runtime, which stalls the first frame that uses each one. Install it with "
                "'xcodebuild -downloadComponent MetalToolchain' and reconfigure.")
    endif()
endif()

# Compiles shaders for the target platform's GPU backend with the shader compiler, then embeds them in a library under
# `namespace`, along with any extra files given after the shaders. Shaders can include Applause's shader headers, such as
# shader_include.sh, and read varying.def.sc from their own folder. A plugin uses this for its own shaders.
function(applause_embed_shaders project include_filename namespace shaders)
    set(library_shader_dir ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../applause/graphics/shaders)
    file(GLOB library_shader_includes ${library_shader_dir}/*.sh)
    set(dependencies ${library_shader_includes} ${APPLAUSE_SHADER_COMPILER_EXECUTABLE}
        ${APPLAUSE_SHADER_COMPILER_TARGET})

    if(WIN32)
        set(shader_target dxbc)
    elseif(APPLE)
        set(shader_target ${APPLAUSE_METAL_SHADER_TARGET})
        set(shader_target_args ${APPLAUSE_METAL_SHADER_ARGS})
    else()
        set(shader_target spirv)
    endif()

    set(shader_bin_dir ${CMAKE_CURRENT_BINARY_DIR}/${project}/${shader_target})
    file(MAKE_DIRECTORY ${shader_bin_dir})
    set(shader_bins)
    foreach(shader ${shaders})
        get_filename_component(shader_name ${shader} NAME_WE)
        get_filename_component(shader_dir ${shader} DIRECTORY)
        file(GLOB shader_includes ${shader_dir}/*.sh ${shader_dir}/varying.def.sc)
        set(shader_type f)
        if(shader_name MATCHES "^v")
            set(shader_type v)
        elseif(shader_name MATCHES "^cs_")
            set(shader_type c)
        endif()

        list(APPEND shader_bins ${shader_bin_dir}/${shader_name})
        add_custom_command(
            OUTPUT ${shader_bin_dir}/${shader_name}
            COMMAND ${APPLAUSE_SHADER_COMPILER_EXECUTABLE} -i ${library_shader_dir} -f ${shader}
                    -o ${shader_bin_dir}/${shader_name} --type ${shader_type} --target ${shader_target}
                    ${shader_target_args}
            DEPENDS ${shader} ${shader_includes} ${dependencies}
        )
    endforeach()
    set(files ${shader_bins} ${ARGN})
    add_embedded_resources(${project} ${include_filename} ${namespace} "${files}")
    target_sources(${project} PRIVATE ${shaders})
    source_group("Shaders" FILES ${shaders})
endfunction()

file(GLOB APPLAUSE_SHADERS ${APPLAUSE_GRAPHICS_DIR}/shaders/[vfc]s_*.sc)
applause_embed_shaders(ApplauseEmbeddedShaders "shaders.h" "applause::shaders" "${APPLAUSE_SHADERS}")

# Graphics
file(GLOB APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/*.cpp ${APPLAUSE_GRAPHICS_DIR}/*.h
     ${APPLAUSE_GRAPHICS_DIR}/gpu/*.h)
list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/gpu/Gpu.cpp ${APPLAUSE_GRAPHICS_DIR}/gpu/GpuShader.cpp)
if(WIN32)
    list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/win32/EmojiWin32.cpp
         ${APPLAUSE_GRAPHICS_DIR}/gpu/GpuD3d11.cpp)
    set(graphics_libraries d2d1 dwrite Windowscodecs d3d11 dxgi d3dcompiler)
elseif(APPLE)
    list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/macos/EmojiMacos.cpp
         ${APPLAUSE_GRAPHICS_DIR}/gpu/GpuMetal.mm)
    find_library(METAL_LIBRARY Metal)
    find_library(METALKIT_LIBRARY MetalKit)
    find_library(QUARTZCORE_LIBRARY QuartzCore)
    find_library(APPKIT_LIBRARY AppKit)
    set(graphics_libraries ${METAL_LIBRARY} ${METALKIT_LIBRARY} ${QUARTZCORE_LIBRARY} ${APPKIT_LIBRARY})
elseif(UNIX)
    list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/linux/EmojiLinux.cpp
         ${APPLAUSE_GRAPHICS_DIR}/gpu/GpuVulkan.cpp)
    find_path(APPLAUSE_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
    if(NOT APPLAUSE_VULKAN_INCLUDE_DIR)
        CPMAddPackage(
            NAME vulkan_headers
            GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
            GIT_TAG v1.4.341
            GIT_SHALLOW TRUE
            DOWNLOAD_ONLY YES
        )
        set(APPLAUSE_VULKAN_INCLUDE_DIR ${vulkan_headers_SOURCE_DIR}/include)
    endif()
    set(graphics_libraries ${Fontconfig_LIBRARIES} ${CMAKE_DL_LIBS})
    set(graphics_includes ${APPLAUSE_VULKAN_INCLUDE_DIR} ${Fontconfig_INCLUDE_DIRS})
endif()

add_library(ApplauseGraphics OBJECT ${APPLAUSE_GRAPHICS_SOURCES})
target_include_directories(ApplauseGraphics PRIVATE ${PROJECT_SOURCE_DIR} ${freetype_SOURCE_DIR}/include)
target_include_directories(ApplauseGraphics SYSTEM PRIVATE ${APPLAUSE_GRAPHICS_DIR}/thirdparty ${graphics_includes})
target_link_libraries(ApplauseGraphics PRIVATE
    ApplausePlatform
    ApplauseEmbeddedShaders
    ApplauseEmbeddedFonts
    freetype
    ${graphics_libraries}
)
target_compile_options(ApplauseGraphics PRIVATE ${APPLAUSE_UI_COMPILE_OPTIONS})
set_target_properties(ApplauseGraphics PROPERTIES ${APPLAUSE_HIDDEN_VISIBILITY})
if(APPLAUSE_GRAPHICS_DEBUG_LOGGING)
    target_compile_definitions(ApplauseGraphics PUBLIC APPLAUSE_GRAPHICS_DEBUG_LOGGING=1)
endif()
if(WIN32)
    target_compile_definitions(ApplauseGraphics PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000)
elseif(APPLE)
    target_compile_options(ApplauseGraphics PRIVATE -fobjc-arc)
elseif(UNIX)
    target_compile_definitions(ApplauseGraphics PRIVATE _LARGEFILE64_SOURCE)
endif()

# Windowing
if(WIN32)
    file(GLOB windowing_platform_sources ${APPLAUSE_WINDOWING_DIR}/win32/*)
    set(windowing_libraries dxgi Shell32 Comctl32)
    file(GLOB cursor_files ${APPLAUSE_WINDOWING_DIR}/cursors/*.cur)
elseif(APPLE)
    file(GLOB windowing_platform_sources ${APPLAUSE_WINDOWING_DIR}/macos/*)
elseif(UNIX)
    file(GLOB windowing_platform_sources ${APPLAUSE_WINDOWING_DIR}/linux/*)
    find_package(X11 REQUIRED COMPONENTS xcb)
    find_library(APPLAUSE_XCB_PRESENT_LIB NAMES xcb-present)
    find_path(APPLAUSE_XCB_PRESENT_INCLUDE_DIR xcb/present.h)
    if(NOT APPLAUSE_XCB_PRESENT_LIB OR NOT APPLAUSE_XCB_PRESENT_INCLUDE_DIR)
        message(FATAL_ERROR "Applause: xcb-present is required on Linux (libxcb-present-dev on Debian and Ubuntu)")
    endif()
    set(windowing_libraries ${X11_LIBRARIES} ${X11_Xrandr_LIB} ${CMAKE_DL_LIBS} ${X11_xcb_LIB} ${APPLAUSE_XCB_PRESENT_LIB})
    set(windowing_includes ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH} ${X11_xcb_INCLUDE_PATH}
        ${APPLAUSE_XCB_PRESENT_INCLUDE_DIR})
    file(GLOB cursor_files ${APPLAUSE_WINDOWING_DIR}/cursors/*.cur)
endif()

add_library(ApplauseWindowing OBJECT
    ${APPLAUSE_WINDOWING_DIR}/Windowing.cpp
    ${APPLAUSE_WINDOWING_DIR}/Windowing.h
    ${APPLAUSE_WINDOWING_DIR}/cursors/CursorFile.h
    ${windowing_platform_sources}
)
target_include_directories(ApplauseWindowing PRIVATE ${PROJECT_SOURCE_DIR} ${windowing_includes})
target_link_libraries(ApplauseWindowing PRIVATE ApplausePlatform ${windowing_libraries})
target_compile_options(ApplauseWindowing PRIVATE ${APPLAUSE_UI_COMPILE_OPTIONS})
target_compile_definitions(ApplauseWindowing PRIVATE APPLAUSE_APPLICATION_NAME="${APPLAUSE_APPLICATION_NAME}")
set_target_properties(ApplauseWindowing PROPERTIES ${APPLAUSE_HIDDEN_VISIBILITY})
if(cursor_files)
    add_embedded_resources(ApplauseEmbeddedCursors "applause_cursors.h" "applause::cursors" "${cursor_files}")
    target_link_libraries(ApplauseWindowing PRIVATE ApplauseEmbeddedCursors)
endif()
if(APPLE)
    target_compile_options(ApplauseWindowing PRIVATE -fobjc-arc)
endif()
