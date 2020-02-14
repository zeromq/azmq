project(test_socket_ops)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES}
                                      libzmq
                                      ${CMAKE_THREAD_LIBS_INIT})

add_catch_test(${PROJECT_NAME})
