set(SOURCES serial.hpp serial_common.hpp)

if(WIN32)
  list(APPEND SOURCES serial_windows.cpp)
else()
  list(APPEND SOURCES serial_nonwindows.cpp)
endif()

add_library(ls-communication-serial OBJECT "${SOURCES}")
target_link_libraries(ls-communication-serial PRIVATE Boost::headers)
