cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(libsubtractive-flags)

project(libsubtractive)

set_compiler_flags(17 TRUE)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

install(FILES include/libsubtractive/libsubtractive.hpp 
		DESTINATION "include/libsubtractive")

find_package(GTest REQUIRED)
enable_testing()
include(CTest)

add_subdirectory(src)
add_subdirectory(tests)
