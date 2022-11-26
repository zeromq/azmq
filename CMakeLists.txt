cmake_minimum_required(VERSION 3.16.3...3.25)
# -- v3.16.3 is the default version in the current ubuntu lts release

project(azmq VERSION 1.1.0 LANGUAGES CXX)

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CMAKE_MASTER_PROJECT YES)
endif ()

# -- options
option(BUILD_TESTING "run unit tests" ${CMAKE_MASTER_PROJECT})
option(BUILD_SHARED_LIBS "use the libzmq static or shared" yes)
option(MORE_WARNINGS "More warnings" ${CMAKE_MASTER_PROJECT})
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" no)

# -- warning and errors template from from
# https://github.com/cpp-best-practices/cpp_starter_project/tree/main/cmake
if (CMAKE_SKIP_INSTALL_RULES)
    add_library(project_warnings INTERFACE)
    if (MORE_WARNINGS)
        include(cmake/CompilerWarnings.cmake)
        set_project_warnings(project_warnings)
    endif ()

    add_library(project_options INTERFACE)
    # XXX target_compile_features(project_options INTERFACE)
    include(cmake/Sanitizers.cmake)
    enable_sanitizers(project_options)
endif ()

# -- config settings --
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- dependencies  --
find_package(
    Boost 1.68
    COMPONENTS system
               date_time
               thread
               chrono
               random
    REQUIRED)

include(FindAzmqLibzmq.cmake)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# --- create library --
add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE Azmq::libzmq Boost::boost ${CMAKE_THREAD_LIBS_INIT})

if (CMAKE_SKIP_INSTALL_RULES)
    target_link_libraries(${PROJECT_NAME} INTERFACE project_options project_warnings)
endif ()

target_include_directories(${PROJECT_NAME} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                     "$<INSTALL_INTERFACE:include>")

if (CMAKE_MASTER_PROJECT AND NOT CMAKE_SKIP_INSTALL_RULES)
    include(AzmqCPack.cmake)

    include(CMakePackageConfigHelpers)

    # generate the version file for the config file
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                     VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)

    # install the public headers
    install(DIRECTORY azmq DESTINATION include)

    install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
    configure_file(${PROJECT_NAME}Config.cmake "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" COPYONLY)
    set(ConfigPackageLocation lib/cmake/${PROJECT_NAME})
    install(EXPORT ${PROJECT_NAME}Targets DESTINATION ${ConfigPackageLocation})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake DESTINATION ${ConfigPackageLocation})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/FindAzmqLibzmq.cmake" DESTINATION ${ConfigPackageLocation})
endif ()

if (BUILD_TESTING)
    enable_testing()
endif()

    message(STATUS "Building Tests. Be sure to check out test/constexpr_tests for constexpr testing")
    add_subdirectory(test)
endif ()

add_subdirectory(doc)
