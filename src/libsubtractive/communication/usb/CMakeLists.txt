set(SOURCES hotplug.cpp hotplug.hpp)

find_path(
  LIBUSBP_INCLUDE_DIRS
  NAMES libusbp.h libusbp.hpp
  PATH_SUFFIXES "libusbp" "libusbp-1"
)

add_library(ls-communication-usb OBJECT "${SOURCES}")
target_link_libraries(ls-communication-usb PRIVATE zmq)
target_include_directories(
  ls-communication-usb PRIVATE "${LIBUSBP_INCLUDE_DIRS}"
)

if(APPLE)
  target_link_libraries(
    ls-communication-usb PUBLIC "-framework CoreFoundation" "-framework IOKit"
  )
  target_link_options(ls-communication-usb PUBLIC "-Wl,-F/Library/Frameworks")
endif()
