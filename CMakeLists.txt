cmake_minimum_required(VERSION 3.16.3)
# -- v3.16.3 is the default version in the current ubuntu lts release

project(azmq LANGUAGES CXX)

# -- options
option(BUILD_TESTING "run unit tests" no)
option(BUILD_SHARED_LIBS "use the libzmq static or shared" yes)
option(MORE_WARNINGS "More warnings" no)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" no)

# -- warning and errors template from from
# https://github.com/cpp-best-practices/cpp_starter_project/tree/main/cmake
add_library(project_warnings INTERFACE)
if (MORE_WARNINGS)
    include(cmake/CompilerWarnings.cmake)
    set_project_warnings(project_warnings)
endif ()
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE)
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

# -- config settings --
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- dependencies  --
find_package(Boost 1.68
        COMPONENTS system date_time thread chrono random
        REQUIRED)

include(FindAzmqLibzmq.cmake)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# --- create library --
add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE
        Azmq::libzmq
        Boost::boost
        ${CMAKE_THREAD_LIBS_INIT}
        project_options
        project_warnings
        )

target_include_directories(${PROJECT_NAME}
        INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
        )

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CMAKE_MASTER_PROJECT YES)
endif ()

if (CMAKE_MASTER_PROJECT)
    include(AzmqCPack.cmake)

    # install the public headers
    install(DIRECTORY azmq DESTINATION include)

    install(TARGETS ${PROJECT_NAME} project_options project_warnings EXPORT ${PROJECT_NAME}Targets
            RUNTIME DESTINATION bin
            ARCHIVE DESTINATION lib)
    configure_file(${PROJECT_NAME}Config.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            COPYONLY
            )
    set(ConfigPackageLocation lib/cmake/${PROJECT_NAME})
    install(EXPORT ${PROJECT_NAME}Targets DESTINATION ${ConfigPackageLocation})
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            DESTINATION ${ConfigPackageLocation})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/FindAzmqLibzmq.cmake"
            DESTINATION ${ConfigPackageLocation})

endif ()


if (BUILD_TESTING)
    enable_testing()
endif()

    macro(add_catch_test name)
        if (TEST_REPORT_FORMAT)
            add_test(NAME ${name} COMMAND ${name} -r ${TEST_REPORT_FORMAT} -o "${name}.test_out.xml")
        else ()
            add_test(NAME ${name} COMMAND ${name})
        endif ()
    endmacro()

    message("Building Tests. Be sure to check out test/constexpr_tests for constexpr testing")
    add_subdirectory(test)
    add_subdirectory(doc)
endif ()
