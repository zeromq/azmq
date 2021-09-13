cmake_minimum_required(VERSION 3.0)
project(azmq-super-build)

#options
set(BOOST_URL "system" CACHE STRING "boost source url")
set(ZMQ_TAG "v4.3.4" CACHE STRING "zmq tag")
option(ZMQ_BUILD_WITH_CMAKE "use cmake (default to autotools)" no)
option(BUILD_SHARED_LIBS "build shared (default to static)" no)

# setup configuration derived from the option
include(ProcessorCount)
ProcessorCount(CPU_COUNT)

if (BUILD_SHARED_LIBS)
    message(STATUS " build with shared libraries")
    set(ZMQ_BUILD_STATIC no)
    set(ZMQ_BUILD_SHARED yes)
else ()
    message(STATUS " build with static libraries")
    set(ZMQ_BUILD_STATIC yes)
    set(ZMQ_BUILD_SHARED no)
endif ()

include(ExternalProject)

set(BOOST_177 "https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.bz2")
set(BOOST_176 "https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.bz2")
set(BOOST_174 "https://boostorg.jfrog.io/artifactory/main/release/1.74.0/source/boost_1_74_0.tar.bz2")
set(BOOST_172 "https://boostorg.jfrog.io/artifactory/main/release/1.72.0/source/boost_1_72_0.tar.bz2")
set(BOOST_170 "https://boostorg.jfrog.io/artifactory/main/release/1.70.0/source/boost_1_70_0.tar.bz2")
set(BOOST_168 "https://boostorg.jfrog.io/artifactory/main/release/1.68.0/source/boost_1_68_0.tar.bz2")

set(DEVROOT ${CMAKE_BINARY_DIR}/devroot)

set(CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Debug
        -DCMAKE_PREFIX_PATH=${DEVROOT}
        -DCMAKE_INSTALL_PREFIX=${DEVROOT}
        -DCMAKE_FIND_ROOT_PATH=${DEVROOT}
        -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
        )

set(LIBS "")
if (NOT (ZMQ_TAG STREQUAL "system"))
    if (ZMQ_BUILD_WITH_CMAKE)
        message(STATUS " build libzmq with cmake ")
        ExternalProject_add(
                zmq
                GIT_REPOSITORY https://github.com/zeromq/libzmq.git
                GIT_TAG ${ZMQ_TAG}
                CMAKE_ARGS ${CMAKE_ARGS}
                -DZMQ_BUILD_TESTS=OFF
                -DWITH_PERF_TOOL=OFF
                -DWITH_LIBSODIUM=OFF
                -DBUILD_STATIC=${ZMQ_BUILD_STATIC}
                -DBUILD_SHARED=${ZMQ_BUILD_SHARED}
        )
    else()
        message(STATUS " build libzmq with autotools ")
        ExternalProject_add(
                zmq
                GIT_REPOSITORY https://github.com/zeromq/libzmq.git
                GIT_TAG ${ZMQ_TAG}

                PATCH_COMMAND ./autogen.sh
                CONFIGURE_COMMAND <SOURCE_DIR>/configure
                --prefix=${DEVROOT}
                --enable-static=${ZMQ_BUILD_STATIC}
                --enable-shared=${ZMQ_BUILD_SHARED}
                BUILD_COMMAND make -j${CPU_COUNT}
                INSTALL_COMMAND make install
        )
    endif ()
    list(APPEND LIBS zmq)
endif ()


if (NOT (BOOST_URL STREQUAL "system"))
    ExternalProject_Add(
            boost
            URL ${${BOOST_URL}}
            BUILD_IN_SOURCE 1
            INSTALL_DIR ${DEVROOT}
            CONFIGURE_COMMAND ./bootstrap.sh --prefix=<INSTALL_DIR>
            BUILD_COMMAND ""
            INSTALL_COMMAND ./b2 install --prefix=<INSTALL_DIR> -j${CPU_COUNT}
    )
    list(APPEND LIBS boost)
endif ()

ExternalProject_Add(
        azmq
        DEPENDS ${LIBS}
        CMAKE_ARGS ${CMAKE_ARGS} -DBUILD_TESTING=YES
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/../../..
        TEST_COMMAND ctest --repeat-until-fail 3
        TEST_BEFORE_INSTALL YES
)
