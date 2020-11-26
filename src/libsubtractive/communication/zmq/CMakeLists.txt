set(SOURCES zeromq_wrapper.cpp zeromq_wrapper.hpp)

add_library(ls-communication-zmq OBJECT "${SOURCES}")
target_link_libraries(ls-communication-zmq)
