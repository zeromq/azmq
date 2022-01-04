project(test_socket_cpp20)
cmake_minimum_required(VERSION 3.16)

include(CheckIncludeFileCXX)
check_include_file_cxx(coroutine FOUND_COROUTINE_HEADER)
set(REQUIRES_MSG "coroutine support")
if (FOUND_COROUTINE_HEADER)
    message(STATUS "${PROJECT_NAME} found ${REQUIRES_MSG}")
else()
    message(WARNING "${PROJECT_NAME} requires ${REQUIRES_MSG}")
    return()
endif ()

set(REQUIRES_MSG "a c++20 compiler")
if (cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(STATUS "${PROJECT_NAME} found ${REQUIRES_MSG}")
else()
    message(WARNING "${PROJECT_NAME} requires ${REQUIRES_MSG}")
    return()
endif ()

set(REQUIRES_MSG "boost version >= 107000")
if (Boost_VERSION_MACRO GREATER_EQUAL 107000)
    message(STATUS "${PROJECT_NAME} found ${REQUIRES_MSG}")
else()
    message(WARNING "${PROJECT_NAME} requires ${REQUIRES_MSG}, but has version : ${Boost_VERSION_MACRO}")
    return()
endif ()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME}
        Azmq::azmq
        Boost::boost
        Boost::thread
        Boost::system
        ${ZeroMQ_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

add_catch_test(${PROJECT_NAME})
