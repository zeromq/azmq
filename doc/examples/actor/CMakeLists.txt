project(actor)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    zeromq::azmq
    ${ZeroMQ_LIBRARIES}
    ${Boost_LIBRARIES}
)

