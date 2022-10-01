cmake_minimum_required(VERSION 3.14)
project(azmq VERSION 1.0.3 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(GNUInstallDirs)
include(AzmqCPack)

find_package(Boost 1.48 COMPONENTS system date_time thread chrono random REQUIRED)
find_package(ZeroMQ 4.0 REQUIRED)

add_library(azmq INTERFACE)
add_library(zeromq::azmq ALIAS azmq)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

option(AZMQ_BUILD_TESTS "Build azmq tests" OFF)
option(AZMQ_BUILD_EXAMPLES "Build azmq examples" OFF)
option(AZMQ_INSTALL "Generate target for installing azmq" ${is_top_level})

if (${CMAKE_CXX_PLATFORM_ID} STREQUAL "Windows")
    # Build for Windows Vista / Windows Server 2008 and later
    add_definitions(-D_WIN32_WINNT=0x0600)

    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)

    if (NOT DEFINED Boost_USE_STATIC_LIBS)
        set(Boost_USE_STATIC_LIBS ON) # On Windows, Boost is compiled as static libraries by default
    endif()
endif()

target_include_directories(azmq
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(azmq
    INTERFACE
    ${Boost_LIBRARIES}
    zeromq::zeromq
)

if(AZMQ_BUILD_EXAMPLES)
    add_subdirectory(doc)
endif()

if(AZMQ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(AZMQ_INSTALL AND NOT CMAKE_SKIP_INSTALL_RULES)
install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/azmq
    DESTINATION include)

set(project_config_in "${CMAKE_CURRENT_LIST_DIR}/cmake/azmqConfig.cmake.in")
set(project_config_out "${CMAKE_CURRENT_BINARY_DIR}/azmqConfig.cmake")
set(config_targets_file "azmqConfigTargets.cmake")
set(version_config_file "${CMAKE_CURRENT_BINARY_DIR}/azmqConfigVersion.cmake")
set(export_dest_dir "${CMAKE_INSTALL_LIBDIR}/cmake/azmq")

# ---------------------------------------------------------------------------------------
# Install CMake config files
# ---------------------------------------------------------------------------------------
install(TARGETS azmq
        EXPORT azmqTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT azmqTargets
        DESTINATION ${export_dest_dir}
        NAMESPACE zeromq::
        FILE ${config_targets_file}
)

include(CMakePackageConfigHelpers)
configure_file("${project_config_in}" "${project_config_out}" @ONLY)

write_basic_package_version_file("${version_config_file}" COMPATIBILITY SameMajorVersion)
install(FILES "${project_config_out}" "${version_config_file}" DESTINATION "${export_dest_dir}")
endif()
