set(SOURCES hotplug.cpp hotplug.hpp)

find_path(
  LIBUSBP_INCLUDE_DIRS
  NAMES 
    libusbp/libusbp.h 
    libusbp/libusbp.hpp
    libusbp-1/libusbp.h
    libusbp-1/libusbp.hpp
)


message(STATUS "LibUSBP Include: ${LIBUSBP_INCLUDE_DIRS}")

add_library(ls-communication-usb OBJECT "${SOURCES}")
target_link_libraries(ls-communication-usb PRIVATE zmq)
target_include_directories(
  ls-communication-usb PRIVATE "${LIBUSBP_INCLUDE_DIRS}"
)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    install(TARGETS ls-communication-usb EXPORT subtractive-targets)
endif()

if(APPLE)
  target_link_libraries(
    ls-communication-usb PUBLIC "-framework CoreFoundation" "-framework IOKit"
  )
  target_link_options(ls-communication-usb PUBLIC "-Wl,-F/Library/Frameworks")
endif()
