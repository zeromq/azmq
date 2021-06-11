find_package(Catch2 REQUIRED)
link_libraries(Catch2::Catch2)

add_subdirectory(message)
add_subdirectory(context_ops)
add_subdirectory(socket_ops)
add_subdirectory(socket)
add_subdirectory(signal)
add_subdirectory(actor)

