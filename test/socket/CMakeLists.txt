project(test_socket)

add_executable(${PROJECT_NAME} main.cpp)

find_package(Boost COMPONENTS coroutine)

target_link_libraries(${PROJECT_NAME} Azmq::azmq Boost::coroutine)

add_catch_test(${PROJECT_NAME})
