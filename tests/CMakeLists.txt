list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(libsubtractive-flags)

add_executable(ExampleTest ExampleTest.cpp)
target_link_libraries(ExampleTest gtest gtest_main)
add_test(exampleGTest ExampleTest)