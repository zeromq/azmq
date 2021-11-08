# Determine the Azmq::libzmq library
#
# This script is a wrapper around find_package and pkg-config
# so inclusion of azmq just works for both install types
#
# If you need different logic define the "Azmq::libzmq" target
# before calling add_subdirectory or find azmq

if (TARGET Azmq::libzmq)
    # allow overriding from the calling script
    message(STATUS "using provided Azmq::libzmq target")
elseif (TARGET libzmq AND BUILD_SHARED_LIBS)
    message(STATUS "using existing shared library target")
    add_library(Azmq::libzmq ALIAS libzmq)
elseif (TARGET libzmq-static AND NOT BUILD_SHARED_LIBS)
    message(STATUS "using existing static library target")
    add_library(Azmq::libzmq ALIAS libzmq-static)
else ()
    # try finding the package
    find_package(ZeroMQ QUIET)
    if (ZeroMQ_FOUND)
        # libzmq exports different targets depending on shared vs static
        # select the right one based upon BUILD_SHARED_LIBS
        if (BUILD_SHARED_LIBS)
            if (TARGET libzmq)
                message(STATUS "using cmake config libzmq")
                add_library(Azmq::libzmq ALIAS libzmq)
            else ()
                message(FATAL_ERROR "libzmq not exported in the cmake configuration")
            endif ()
        else ()
            if (TARGET libzmq-static)
                message(STATUS "using cmake config static libzmq")
                add_library(Azmq::libzmq ALIAS libzmq-static)
            else ()
                message(FATAL_ERROR "libzmq-static not exported in the cmake configuration")
            endif ()
        endif ()
    else ()
        # fallback to pkg-config
        message(STATUS "CMake libzmq package not found, trying again with pkg-config (normal install of zeromq)")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBZMQ REQUIRED IMPORTED_TARGET GLOBAL libzmq)
        if (LIBZMQ_FOUND)
            add_library(Azmq::libzmq ALIAS PkgConfig::LIBZMQ)
        else ()
            message(FAIL_ERROR "Can't find the required libzmq library")
        endif ()
    endif ()
endif ()