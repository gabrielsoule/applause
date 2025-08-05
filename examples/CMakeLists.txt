# Examples for Applause CLAP Plugin Framework

add_custom_target(applause-examples
    COMMENT "Building all Applause examples"
)

# Helper function to automatically add all plugin examples
function(add_applause_examples)
    # Get all subdirectories that contain CMakeLists.txt
    file(GLOB example_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
    
    foreach(dir ${example_dirs})
        if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dir} AND 
           EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/CMakeLists.txt)
            message(STATUS "Applause: Adding example '${dir}'")
            add_subdirectory(${dir})
        endif()
    endforeach()
endfunction()

add_applause_examples()