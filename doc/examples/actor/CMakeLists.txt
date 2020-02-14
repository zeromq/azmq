project(actor)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT}
                                      libzmq
                                      ${Boost_LIBRARIES})

