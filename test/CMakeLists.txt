find_package(Catch2 2.13.7 QUIET)
if(TARGET Catch2::Catch2)
    message(STATUS "Using system installed Catch2 v${Catch2_VERSION}" )
else()
    message(STATUS "Fetching Catch2 from github")
    Include(FetchContent)
    FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v2.13.7)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)
endif()

link_libraries(Catch2::Catch2)

macro(add_catch_test name)
    include(CTest)
    include(Catch)
    catch_discover_tests(${name})
endmacro()

add_subdirectory(message)
add_subdirectory(context_ops)
add_subdirectory(socket_ops)
add_subdirectory(socket)
add_subdirectory(signal)
add_subdirectory(actor)

