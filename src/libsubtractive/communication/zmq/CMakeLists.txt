set(SOURCES zeromq_wrapper.cpp zeromq_wrapper.hpp)

add_library(ls-communication-zmq OBJECT "${SOURCES}")
target_link_libraries(ls-communication-zmq)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    install(TARGETS ls-communication-zmq EXPORT subtractive-targets)
endif()
