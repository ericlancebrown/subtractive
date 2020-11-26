add_subdirectory(serial)
add_subdirectory(usb)
add_subdirectory(zmq)

set(SOURCES flowcontrol.cpp flowcontrol.hpp)

add_library(ls-communication OBJECT "${SOURCES}")
target_link_libraries(ls-communication PRIVATE Boost::headers)
