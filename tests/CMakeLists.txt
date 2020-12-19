list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(libsubtractive-flags)

find_path(
  GTEST_INCLUDE_DIRS
  NAMES 
    gtest/gtest.h
    gtest/gmock.h
)

message(STATUS "GTest Include: ${GTEST_INCLUDE_DIRS}")


find_library(
  GTEST_LIBRARIES
  NAMES 
    gtest
  HINTS
    ${CMAKE_FIND_ROOT_PATH}
  PATH_SUFFIXES 
    "lib" "lib32" "lib64"
)
message(STATUS "GTest Library: ${GTEST_LIBRARIES}")

add_executable(ExampleTest ExampleTest.cpp)
target_include_directories(ExampleTest PRIVATE "${GTEST_INCLUDE_DIRS}")
target_link_libraries(ExampleTest "${GTEST_LIBRARIES}")
add_test(exampleGTest ExampleTest)