# ApplausePlugin.cmake - Simplified CMake API for creating Applause-based CLAP plugins
#
# Usage:
#   add_applause_plugin(
#       TARGET_NAME MyPlugin
#       OUTPUT_NAME "My Plugin"
#       PLUGIN_CLASS MyPluginClass
#       PLUGIN_HEADER "MyPluginClass.h"
#       
#       # Plugin descriptor
#       PLUGIN_ID "com.example.myplugin"
#       PLUGIN_NAME "My Plugin"
#       PLUGIN_VENDOR "My Company"
#       PLUGIN_URL "https://example.com/myplugin"  # Optional
#       PLUGIN_MANUAL_URL ""  # Optional
#       PLUGIN_SUPPORT_URL ""  # Optional
#       PLUGIN_VERSION "1.0.0"
#       PLUGIN_DESCRIPTION "A great plugin"
#       PLUGIN_FEATURES INSTRUMENT SYNTHESIZER  # List of CLAP features without CLAP_PLUGIN_FEATURE_ prefix
#       
#       # Implementation sources
#       SOURCES src/MyPlugin.cpp src/MyPlugin.h src/MyEditor.cpp src/MyEditor.h
#       
#       # Plugin formats to generate
#       PLUGIN_FORMATS CLAP VST3 AUV2
#       
#       # Bundle information
#       BUNDLE_IDENTIFIER "com.example.myplugin"
#       BUNDLE_VERSION "1.0.0"
#       
#       # AUV2 specific (required if AUV2 is in PLUGIN_FORMATS)
#       AUV2_MANUFACTURER_NAME "MyCo"
#       AUV2_MANUFACTURER_CODE "MyC1"
#       AUV2_SUBTYPE_CODE "MyPl"
#       AUV2_INSTRUMENT_TYPE "aumu"
#       
#       # Optional
#       COPY_AFTER_BUILD TRUE
#   )

function(add_applause_plugin)
    # Parse arguments
    cmake_parse_arguments(
        APPL
        "COPY_AFTER_BUILD"  # Options
        "TARGET_NAME;OUTPUT_NAME;PLUGIN_CLASS;PLUGIN_HEADER;PLUGIN_ID;PLUGIN_NAME;PLUGIN_VENDOR;PLUGIN_URL;PLUGIN_MANUAL_URL;PLUGIN_SUPPORT_URL;PLUGIN_VERSION;PLUGIN_DESCRIPTION;BUNDLE_IDENTIFIER;BUNDLE_VERSION;AUV2_MANUFACTURER_NAME;AUV2_MANUFACTURER_CODE;AUV2_SUBTYPE_CODE;AUV2_INSTRUMENT_TYPE"  # Single-value args
        "PLUGIN_FEATURES;SOURCES;PLUGIN_FORMATS"  # Multi-value args
        ${ARGN}
    )
    
    # Validate required arguments
    if(NOT APPL_TARGET_NAME)
        message(FATAL_ERROR "applause_plugin: TARGET_NAME is required")
    endif()
    if(NOT APPL_PLUGIN_CLASS)
        message(FATAL_ERROR "applause_plugin: PLUGIN_CLASS is required")
    endif()
    if(NOT APPL_PLUGIN_ID)
        message(FATAL_ERROR "applause_plugin: PLUGIN_ID is required")
    endif()
    if(NOT APPL_PLUGIN_NAME)
        message(FATAL_ERROR "applause_plugin: PLUGIN_NAME is required")
    endif()
    if(NOT APPL_SOURCES)
        message(FATAL_ERROR "applause_plugin: SOURCES is required")
    endif()
    
    # Set defaults
    if(NOT APPL_OUTPUT_NAME)
        set(APPL_OUTPUT_NAME "${APPL_TARGET_NAME}")
    endif()
    if(NOT APPL_PLUGIN_VENDOR)
        set(APPL_PLUGIN_VENDOR "Unknown Vendor")
    endif()
    if(NOT APPL_PLUGIN_VERSION)
        set(APPL_PLUGIN_VERSION "1.0.0")
    endif()
    if(NOT APPL_PLUGIN_DESCRIPTION)
        set(APPL_PLUGIN_DESCRIPTION "${APPL_PLUGIN_NAME}")
    endif()
    if(NOT APPL_BUNDLE_IDENTIFIER)
        set(APPL_BUNDLE_IDENTIFIER "${APPL_PLUGIN_ID}")
    endif()
    if(NOT APPL_BUNDLE_VERSION)
        set(APPL_BUNDLE_VERSION "${APPL_PLUGIN_VERSION}")
    endif()
    if(NOT APPL_PLUGIN_FORMATS)
        set(APPL_PLUGIN_FORMATS CLAP)
    endif()
    
    # Generate the plugin entry C++ file
    set(ENTRY_FILE "${CMAKE_CURRENT_BINARY_DIR}/${APPL_TARGET_NAME}_generated_entry.cpp")
    
    # Build the features array
    set(FEATURES_ARRAY "")
    foreach(feature ${APPL_PLUGIN_FEATURES})
        string(APPEND FEATURES_ARRAY "    CLAP_PLUGIN_FEATURE_${feature},\n")
    endforeach()
    string(APPEND FEATURES_ARRAY "    nullptr")
    
    # Handle optional URLs
    if(APPL_PLUGIN_URL)
        set(URL_VALUE "\"${APPL_PLUGIN_URL}\"")
    else()
        set(URL_VALUE "nullptr")
    endif()
    
    if(APPL_PLUGIN_MANUAL_URL)
        set(MANUAL_URL_VALUE "\"${APPL_PLUGIN_MANUAL_URL}\"")
    else()
        set(MANUAL_URL_VALUE "nullptr")
    endif()
    
    if(APPL_PLUGIN_SUPPORT_URL)
        set(SUPPORT_URL_VALUE "\"${APPL_PLUGIN_SUPPORT_URL}\"")
    else()
        set(SUPPORT_URL_VALUE "nullptr")
    endif()
    
    # Generate the entry file content
    set(ENTRY_CONTENT "#include \"${APPL_PLUGIN_HEADER}\"
#include <clap/clap.h>
#include <cstring>
#include \"util/DebugHelpers.h\"

// Plugin features
static const char* features[] = {
${FEATURES_ARRAY}
};

// Plugin descriptor
static const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION,
    .id = \"${APPL_PLUGIN_ID}\",
    .name = \"${APPL_PLUGIN_NAME}\",
    .vendor = \"${APPL_PLUGIN_VENDOR}\",
    .url = ${URL_VALUE},
    .manual_url = ${MANUAL_URL_VALUE},
    .support_url = ${SUPPORT_URL_VALUE},
    .version = \"${APPL_PLUGIN_VERSION}\",
    .description = \"${APPL_PLUGIN_DESCRIPTION}\",
    .features = features
};

// Plugin factory implementation
static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* factory,
    const clap_host_t* host,
    const char* plugin_id) {
    
    if (!plugin_id || std::strcmp(plugin_id, descriptor.id) != 0) {
        return nullptr;
    }
    
    auto* plugin = new ${APPL_PLUGIN_CLASS}(&descriptor, host);
    return plugin->clapPlugin();
}

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* factory,
    uint32_t index) {
    
    if (index == 0) {
        return &descriptor;
    }
    return nullptr;
}

static const clap_plugin_factory_t generated_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};

// Plugin entry point implementation
static bool clap_init(const char* path) {
    return true;
}

static void clap_deinit() {
}

static const void* clap_get_factory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &generated_factory;
    }
    return nullptr;
}

// Export the plugin entry point
extern \"C\" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = clap_init,
    .deinit = clap_deinit,
    .get_factory = clap_get_factory
};
")
    
    # Write the generated entry file
    file(WRITE "${ENTRY_FILE}" "${ENTRY_CONTENT}")
    
    # Create the implementation library
    add_library(${APPL_TARGET_NAME}-impl STATIC ${APPL_SOURCES})
    
    # Link with Applause
    target_link_libraries(${APPL_TARGET_NAME}-impl PUBLIC Applause::Applause)
    
    # Include directories
    target_include_directories(${APPL_TARGET_NAME}-impl PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    # Platform specific definitions
    if(WIN32)
        target_compile_definitions(${APPL_TARGET_NAME}-impl PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()
    
    # Call make_clapfirst_plugins with all the arguments
    set(MAKE_CLAPFIRST_ARGS
        TARGET_NAME ${APPL_TARGET_NAME}
        IMPL_TARGET ${APPL_TARGET_NAME}-impl
        OUTPUT_NAME "${APPL_OUTPUT_NAME}"
        ENTRY_SOURCE "${ENTRY_FILE}"
        BUNDLE_IDENTIFIER "${APPL_BUNDLE_IDENTIFIER}"
        BUNDLE_VERSION ${APPL_BUNDLE_VERSION}
        PLUGIN_FORMATS ${APPL_PLUGIN_FORMATS}
    )
    
    # Add optional arguments
    if(APPL_COPY_AFTER_BUILD)
        list(APPEND MAKE_CLAPFIRST_ARGS COPY_AFTER_BUILD TRUE)
    endif()
    
    # Add AUV2 specific arguments if AUV2 is in formats
    if("AUV2" IN_LIST APPL_PLUGIN_FORMATS)
        if(NOT APPL_AUV2_MANUFACTURER_NAME OR NOT APPL_AUV2_MANUFACTURER_CODE OR NOT APPL_AUV2_SUBTYPE_CODE OR NOT APPL_AUV2_INSTRUMENT_TYPE)
            message(FATAL_ERROR "applause_plugin: When AUV2 is in PLUGIN_FORMATS, all AUV2_* parameters are required")
        endif()
        
        list(APPEND MAKE_CLAPFIRST_ARGS
            AUV2_MANUFACTURER_NAME "${APPL_AUV2_MANUFACTURER_NAME}"
            AUV2_MANUFACTURER_CODE "${APPL_AUV2_MANUFACTURER_CODE}"
            AUV2_SUBTYPE_CODE "${APPL_AUV2_SUBTYPE_CODE}"
            AUV2_INSTRUMENT_TYPE "${APPL_AUV2_INSTRUMENT_TYPE}"
        )
    endif()
    
    # Call the underlying function
    make_clapfirst_plugins(${MAKE_CLAPFIRST_ARGS})
    
    # Add macOS post-build signing for VST3 if applicable
    if(APPLE AND "VST3" IN_LIST APPL_PLUGIN_FORMATS)
        add_custom_command(TARGET ${APPL_TARGET_NAME}_vst3 POST_BUILD
            COMMAND codesign --force --sign - --timestamp=none
            "$<TARGET_FILE_DIR:${APPL_TARGET_NAME}_vst3>/../.."
            COMMENT "Ad-hoc signing build directory VST3 bundle"
        )
    endif()
    
    # Add to parent target if it exists
    if(TARGET applause-examples)
        add_dependencies(applause-examples ${APPL_TARGET_NAME}_all)
    endif()
    
endfunction()