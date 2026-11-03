file(GLOB_RECURSE APPLAUSE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
if(APPLE)
    file(GLOB_RECURSE APPLAUSE_OBJCXX_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")
    list(APPEND APPLAUSE_TEST_SOURCES ${APPLAUSE_OBJCXX_TEST_SOURCES})
endif()

add_executable(applause_tests ${APPLAUSE_TEST_SOURCES})

file(GLOB APPLAUSE_TEST_SHADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/shaders/[vf]s_*.sc")
applause_embed_shaders(ApplauseTestShaders "test_shaders.h" "applause::test_shaders" "${APPLAUSE_TEST_SHADERS}")

target_link_libraries(applause_tests
    PRIVATE
        Applause::Applause
        ApplauseTestShaders
        Catch2::Catch2WithMain
)

# Tests that own the X11 clipboard selection can't run alongside each other.
catch_discover_tests(applause_tests TEST_SPEC "~[x11_selection]")
catch_discover_tests(applause_tests TEST_SPEC "[x11_selection]" PROPERTIES RESOURCE_LOCK applause_x11_selection)
