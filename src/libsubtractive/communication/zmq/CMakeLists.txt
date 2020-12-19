set(SOURCES zeromq_wrapper.cpp zeromq_wrapper.hpp)

find_path(
  ZEROMQ_INCLUDE_DIRS
  NAMES 
    zmq.h
    zeromq/include
)

message(STATUS "ZeroMQ Include: ${ZEROMQ_INCLUDE_DIRS}")

find_library(
  ZEROMQ_LIBRARIES
  NAMES 
    zmq
  HINTS
    ${CMAKE_FIND_ROOT_PATH}
  PATH_SUFFIXES 
    "lib" "lib32" "lib64"
)
message(STATUS "ZeroMQ Library: ${ZEROMQ_LIBRARIES}")

add_library(ls-communication-zmq OBJECT "${SOURCES}")
target_include_directories(
  ls-communication-zmq PRIVATE "${ZEROMQ_INCLUDE_DIRS}"
)
target_link_libraries(ls-communication-zmq PRIVATE ${ZEROMQ_LIBRARIES})

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    install(TARGETS ls-communication-zmq EXPORT subtractive-targets)
endif()
