include(GNUInstallDirs)

find_package(Boost REQUIRED system thread)

add_subdirectory(communication)

set(sources
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
add_library(subtractive SHARED "${sources}")
add_library(libsubtractive ALIAS subtractive)

find_library(
  LIBUSBP_LIBRARIES
  NAMES 
    usbp-1
  HINTS
    ${CMAKE_FIND_ROOT_PATH}
  PATH_SUFFIXES 
    "lib" "lib32" "lib64"
)
message(STATUS "LibUSBP Library: ${LIBUSBP_LIBRARIES}")

if(UNIX AND NOT APPLE)
  list(APPEND LIBUSBP_LIBRARIES "-ludev")
endif()

target_link_libraries(
    subtractive
  PUBLIC 
    "${LIBUSBP_LIBRARIES}" 
    "${Boost_SYSTEM_LIBRARY}"
    "${Boost_HEADERS_LIBRARY}"
  INTERFACE 
    ls-communication-usb
)

# Install target
install(FILES "libsubtractive.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# install(TARGETS subtractive LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

set_target_properties(subtractive PROPERTIES
    FRAMEWORK       FALSE
    VERSION             ${subtractive_VERSION}
    SOVERSION           ${subtractive_SOVERSION}
    DEFINE_SYMBOL       MAKE_CBANG_LIB
)
install(TARGETS subtractive
    EXPORT          subtractive-targets
    RUNTIME         DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY         DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE         DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libsubtractive
    PUBLIC_HEADER   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libsubtractive
)
install(EXPORT subtractive-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/subtractive
)
configure_package_config_file(
    ${libsubtractive_SOURCE_DIR}/cmake/subtractiveConfig.cmake.in ${CMAKE_BINARY_DIR}/subtractiveConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/subtractive
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)
write_basic_package_version_file(${CMAKE_BINARY_DIR}/subtractiveConfigVersion.cmake
    VERSION ${subtractive_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_BINARY_DIR}/subtractiveConfig.cmake
    ${CMAKE_BINARY_DIR}/subtractiveConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/subtractive
)
