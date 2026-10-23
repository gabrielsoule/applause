# Graphics and windowing (derived from Visage), with bgfx, freetype and shaders.
# These build as their own targets so their platform code compiles with exactly the flags upstream uses.

option(APPLAUSE_BACKGROUND_GRAPHICS_THREAD "Offloads graphics rendering to a background thread" OFF)
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

# bgfx and freetype
set(BGFX_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(BGFX_INSTALL OFF CACHE INTERNAL "" FORCE)
set(BX_AMALGAMATED ON CACHE INTERNAL "" FORCE)
set(BX_CONFIG_DEBUG OFF CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_DEBUG_ANNOTATION ${APPLAUSE_GRAPHICS_DEBUG_LOGGING} CACHE INTERNAL "" FORCE)
set(BGFX_AMALGAMATED ON CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MAX_FRAME_BUFFERS 1024 CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MAX_VIEWS 256 CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MAX_DRAW_CALLS 8192 CACHE INTERNAL "" FORCE)
set(BGFX_WITH_WAYLAND OFF CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MULTITHREADED 0 CACHE INTERNAL "" FORCE)
if(APPLAUSE_BACKGROUND_GRAPHICS_THREAD)
    set(BGFX_CONFIG_MULTITHREADED 1 CACHE INTERNAL "" FORCE)
endif()
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)

set(CMAKE_MESSAGE_LOG_LEVEL ERROR)
CPMAddPackage(
    NAME bgfx
    GITHUB_REPOSITORY bkaradzic/bgfx.cmake
    GIT_TAG v1.129.8958-499
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
    NAME freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-14-1
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
)
unset(CMAKE_MESSAGE_LOG_LEVEL)

foreach(dependency freetype bgfx bx bimg bimg_decode bimg_encode)
    target_compile_options(${dependency} PRIVATE $<IF:$<BOOL:${MSVC}>,/W0,-w>)
    if(UNIX AND NOT APPLE)
        target_compile_definitions(${dependency} PRIVATE _LARGEFILE64_SOURCE)
    endif()
endforeach()
target_compile_definitions(bx PRIVATE BX_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH=0)

# Compiles bgfx shaders for the target platform's backend with the vendored shaderc, then embeds them in a library
# under `namespace`, along with any extra files given after the shaders. Shaders can include Applause's shader headers,
# such as shader_include.sh. A plugin uses this for its own shaders.
function(applause_embed_shaders project include_filename namespace shaders)
    set(graphics_dir ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../applause/graphics)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(shaderc ${graphics_dir}/bin/win32/shaderc.exe)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(shaderc ${graphics_dir}/bin/macos/shaderc)
    else()
        set(shaderc ${graphics_dir}/bin/linux/shaderc)
    endif()

    if(WIN32)
        set(shader_args -O3 --platform windows -p s_4_0)
    elseif(APPLE)
        set(shader_args --platform osx -p metal)
    else()
        set(shader_args --platform linux -p spirv)
    endif()

    set(shader_bin_dir ${CMAKE_CURRENT_BINARY_DIR}/${project})
    file(MAKE_DIRECTORY ${shader_bin_dir})
    set(shader_bins)
    foreach(shader ${shaders})
        get_filename_component(shader_name ${shader} NAME_WE)
        if(shader_name MATCHES "^fs_")
            set(shader_type f)
        else()
            set(shader_type v)
        endif()
        list(APPEND shader_bins ${shader_bin_dir}/${shader_name})
        add_custom_command(
            OUTPUT ${shader_bin_dir}/${shader_name}
            COMMAND ${shaderc} -i ${graphics_dir}/shaders -f ${shader} -o ${shader_bin_dir}/${shader_name}
                    --type ${shader_type} ${shader_args}
            DEPENDS ${shader}
        )
    endforeach()
    set(files ${shader_bins} ${ARGN})
    add_embedded_resources(${project} ${include_filename} ${namespace} "${files}")
endfunction()

file(GLOB APPLAUSE_SHADERS ${APPLAUSE_GRAPHICS_DIR}/shaders/[vf]s_*.sc)
file(GLOB APPLAUSE_SHADER_INCLUDES ${APPLAUSE_GRAPHICS_DIR}/shaders/*.sh ${APPLAUSE_GRAPHICS_DIR}/shaders/varying.def.sc)
applause_embed_shaders(ApplauseEmbeddedShaders "shaders.h" "applause::shaders" "${APPLAUSE_SHADERS}"
                       ${APPLAUSE_SHADER_INCLUDES} ${APPLAUSE_SHADERS})

# Graphics
file(GLOB APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/*.cpp ${APPLAUSE_GRAPHICS_DIR}/*.h)
if(WIN32)
    list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/win32/EmojiWin32.cpp)
    set(graphics_libraries d2d1 dwrite Windowscodecs)
elseif(APPLE)
    list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/macos/EmojiMacos.cpp
         ${APPLAUSE_GRAPHICS_DIR}/macos/WindowlessContext.mm)
    find_library(METAL_LIBRARY Metal)
    find_library(METALKIT_LIBRARY MetalKit)
    set(graphics_libraries ${METAL_LIBRARY} ${METALKIT_LIBRARY})
elseif(UNIX)
    list(APPEND APPLAUSE_GRAPHICS_SOURCES ${APPLAUSE_GRAPHICS_DIR}/linux/EmojiLinux.cpp)
endif()

add_library(ApplauseGraphics OBJECT ${APPLAUSE_GRAPHICS_SOURCES})
target_include_directories(ApplauseGraphics PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${freetype_SOURCE_DIR}/include
    ${BGFX_DIR}/include
    ${BGFX_DIR}/3rdparty
    ${BX_DIR}/include
    ${BIMG_DIR}/include
)
target_include_directories(ApplauseGraphics SYSTEM PRIVATE ${APPLAUSE_GRAPHICS_DIR}/thirdparty)
target_link_libraries(ApplauseGraphics PRIVATE
    ApplausePlatform
    ApplauseEmbeddedShaders
    ApplauseEmbeddedFonts
    bx
    bgfx
    bx
    bimg_decode
    freetype
    ${graphics_libraries}
)
target_compile_options(ApplauseGraphics PRIVATE ${APPLAUSE_UI_COMPILE_OPTIONS})
if(APPLAUSE_GRAPHICS_DEBUG_LOGGING)
    target_compile_definitions(ApplauseGraphics PUBLIC APPLAUSE_GRAPHICS_DEBUG_LOGGING=1)
endif()
if(APPLAUSE_BACKGROUND_GRAPHICS_THREAD)
    target_compile_definitions(ApplauseGraphics PUBLIC APPLAUSE_BACKGROUND_GRAPHICS_THREAD=1)
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
    set(windowing_libraries dxgi Shell32)
elseif(APPLE)
    file(GLOB windowing_platform_sources ${APPLAUSE_WINDOWING_DIR}/macos/*)
elseif(UNIX)
    file(GLOB windowing_platform_sources ${APPLAUSE_WINDOWING_DIR}/linux/*)
    find_package(X11 REQUIRED)
    set(windowing_libraries ${X11_LIBRARIES} ${X11_Xrandr_LIB})
    set(windowing_includes ${X11_INCLUDE_DIR})
endif()

add_library(ApplauseWindowing OBJECT
    ${APPLAUSE_WINDOWING_DIR}/Windowing.cpp
    ${APPLAUSE_WINDOWING_DIR}/Windowing.h
    ${windowing_platform_sources}
)
target_include_directories(ApplauseWindowing PRIVATE ${PROJECT_SOURCE_DIR} ${windowing_includes})
target_link_libraries(ApplauseWindowing PRIVATE ApplausePlatform ${windowing_libraries})
target_compile_options(ApplauseWindowing PRIVATE ${APPLAUSE_UI_COMPILE_OPTIONS})
target_compile_definitions(ApplauseWindowing PRIVATE APPLAUSE_APPLICATION_NAME="${APPLAUSE_APPLICATION_NAME}")
if(APPLE)
    target_compile_options(ApplauseWindowing PRIVATE -fobjc-arc)
endif()
