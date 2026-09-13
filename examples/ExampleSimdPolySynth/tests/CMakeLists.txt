add_executable(ExampleSimdPolySynth_tests
    SimdAdsrTest.cpp
    SimdOscillatorTest.cpp
)

target_include_directories(ExampleSimdPolySynth_tests PRIVATE ../src)

target_link_libraries(ExampleSimdPolySynth_tests
    PRIVATE
        Applause::Applause
        Catch2::Catch2WithMain
)

catch_discover_tests(ExampleSimdPolySynth_tests)
