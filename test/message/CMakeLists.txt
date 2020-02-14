project(test_message)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES}
                                      libzmq)

add_catch_test(${PROJECT_NAME})
