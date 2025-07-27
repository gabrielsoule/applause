# Examples for Applause CLAP Plugin Framework

# Create a custom target to build all examples at once
# This needs to be created BEFORE processing subdirectories so that
# the individual example CMakeLists.txt files can add dependencies to it
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

# Add all examples
add_applause_examples()