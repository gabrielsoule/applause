CPMAddPackage("gh:catchorg/Catch2@3.7.1")
include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

file(GLOB_RECURSE APPLAUSE_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(applause_tests ${APPLAUSE_TEST_SOURCES})

target_link_libraries(applause_tests
    PRIVATE
        Applause::Applause
        Catch2::Catch2WithMain
)

catch_discover_tests(applause_tests)
