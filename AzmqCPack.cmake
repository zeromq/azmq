set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AZMQ Boost Asio + ZeroMQ")
set(CPACK_PACKAGE_DESCRIPTION
"The azmq library provides Boost Asio style bindings for ZeroMQ

This library is built on top of ZeroMQ's standard C interface and is
intended to work well with C++ applications which use the Boost libraries
in general, and Asio in particular.

The main abstraction exposed by the library is azmq::socket which
provides an Asio style socket interface to the underlying zeromq socket
and interfaces with Asio's io_service().  The socket implementation
participates in the io_service's reactor for asynchronous IO and
may be freely mixed with other Asio socket types (raw TCP/UDP/Serial/etc.).")
set(CPACK_PACKAGE_VERSION               "1.1.0")

# Debian-specific packaging
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE   "all")
set(CPACK_DEBIAN_PACKAGE_DEPENDS        "libzmq3-dev (>= 4.0.0), libboost-dev (>= 1.48.0)")
set(CPACK_DEBIAN_PACKAGE_NAME           "libazmq-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION        "libdevel")

# RPM-specific packaging
set(CPACK_RPM_PACKAGE_ARCHITECTURE      "noarch")
set(CPACK_RPM_PACKAGE_DESCRIPTION       "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_GROUP             "Development/Libraries")
set(CPACK_RPM_PACKAGE_LICENSE           "Boost")
set(CPACK_RPM_PACKAGE_NAME              "azmq-devel")
set(CPACK_RPM_PACKAGE_REQUIRES          "zeromq-devel >= 4.0.0")

configure_file("${azmq_SOURCE_DIR}/AzmqCPackOptions.cmake.in" "${azmq_BINARY_DIR}/AzmqCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${azmq_BINARY_DIR}/AzmqCPackOptions.cmake")

include(CPack)
