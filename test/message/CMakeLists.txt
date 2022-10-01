project(test_message)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    zeromq::azmq
)

add_catch_test(${PROJECT_NAME})
