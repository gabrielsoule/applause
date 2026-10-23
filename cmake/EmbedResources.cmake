# add_embedded_resources(<target> <header> <namespace> <files>)
# Builds a static library that embeds each file as an applause::EmbeddedFile named after it (Lato-Regular.ttf becomes
# Lato_Regular_ttf), declared in <embedded/<header>> inside <namespace>.
function(add_embedded_resources project include_filename namespace files)
    get_filename_component(applause_root ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.. ABSOLUTE)
    set(scripts ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed)
    set(generated_path ${CMAKE_CURRENT_BINARY_DIR}/${project}_generated)
    set(destination_path ${generated_path}/embedded)
    file(MAKE_DIRECTORY ${destination_path})

    set(lookup_file ${destination_path}/embedded_files0.cpp)
    set(source_files ${lookup_file})

    set(index 1)
    foreach(file IN LISTS files)
        set(embedded_file_name embedded_files${index}.cpp)
        set(source_file ${destination_path}/${embedded_file_name})
        list(APPEND source_files ${source_file})
        get_filename_component(original_file_name ${file} NAME)
        add_custom_command(
            OUTPUT ${source_file}
            COMMAND ${CMAKE_COMMAND} -DDEST_FILE=${source_file} -DORIGINAL_FILE=${file} -DVAR_NAMESPACE=${namespace}
                    -P ${scripts}/embed_file.cmake
            DEPENDS ${file}
            COMMENT "Generating ${embedded_file_name} for ${original_file_name}"
        )
        math(EXPR index "${index} + 1")
    endforeach()

    string(REPLACE ";" "\\;" escaped_files "${files}")
    add_custom_command(
        OUTPUT ${destination_path}/${include_filename} ${lookup_file}
        COMMAND ${CMAKE_COMMAND} -DDEST_FILE=${destination_path}/${include_filename} -DFILE_LIST="${escaped_files}"
                -DVAR_NAMESPACE=${namespace} -P ${scripts}/create_embed_header.cmake
        COMMAND ${CMAKE_COMMAND} -DDEST_FILE=${lookup_file} -DINCLUDE_FILE=${include_filename}
                -DFILE_LIST="${escaped_files}" -DVAR_NAMESPACE=${namespace} -P ${scripts}/create_file_lookup.cmake
        DEPENDS ${files} ${scripts}/create_embed_header.cmake
        COMMENT "Generating C++ for embedded files..."
    )

    add_library(${project} STATIC ${destination_path}/${include_filename} ${source_files})
    target_include_directories(${project} PUBLIC ${generated_path} ${applause_root})
endfunction()
