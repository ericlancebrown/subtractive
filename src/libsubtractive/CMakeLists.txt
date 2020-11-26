find_package(Boost REQUIRED system thread)

add_subdirectory(communication)

set(SOURCES
    actor.hpp
    context.cpp
    context.hpp
    machine.cpp
    machine.hpp
    $<TARGET_OBJECTS:ls-communication>
    $<TARGET_OBJECTS:ls-communication-serial>
    $<TARGET_OBJECTS:ls-communication-usb>
    $<TARGET_OBJECTS:ls-communication-zmq>
)
add_library(subtractive SHARED "${SOURCES}")
add_library(libsubtractive ALIAS subtractive)

find_library(
  LIBUSBP_LIBRARIES
  NAMES usbp-1
  PATH_SUFFIXES "lib" "lib32" "lib64"
)

if(UNIX AND NOT APPLE)
  list(APPEND LIBUSBP_LIBRARIES "-ludev")
endif()

target_link_libraries(
  subtractive
  PUBLIC "${LIBUSBP_LIBRARIES}" Boost::system Boost::headers
  INTERFACE ls-communication-usb
)

# Install target
install(FILES "libsubtractive.pc" DESTINATION lib/pkgconfig)

install(TARGETS subtractive LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)