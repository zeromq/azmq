cmake_minimum_required(VERSION 3.16.3...3.25)
# -- v3.16.3 is the default version in the current ubuntu lts release

project(azmq VERSION 1.1.0 LANGUAGES CXX)

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CMAKE_MASTER_PROJECT YES)
endif ()

# -- options
option(AZMQ_BUILD_TESTS "run unit tests" ${CMAKE_MASTER_PROJECT})
option(BUILD_SHARED_LIBS "use the libzmq static or shared" yes)
option(AZMQ_DEVELOPER "Add verbose warning for developers " no)

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
add_library(Azmq::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} INTERFACE Azmq::libzmq Boost::boost ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${PROJECT_NAME} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                     "$<INSTALL_INTERFACE:include>")

if (AZMQ_DEVELOPER)
  include(cmake/CompilerWarnings.cmake)
  add_more_warnings_to(${PROJECT_NAME})
endif()

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
    install(EXPORT ${PROJECT_NAME}Targets NAMESPACE Azmq:: DESTINATION ${ConfigPackageLocation})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake DESTINATION ${ConfigPackageLocation})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/FindAzmqLibzmq.cmake" DESTINATION ${ConfigPackageLocation})
endif ()

if (AZMQ_BUILD_TESTS)
    enable_testing()
    message(STATUS "Building Tests. Be sure to check out test/constexpr_tests for constexpr testing")
    add_subdirectory(test)
endif ()

add_subdirectory(doc)
