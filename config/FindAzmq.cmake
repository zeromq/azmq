# Find Azmq Headers/Libs

# Variables
# AZMQ_ROOT - set this to a location where ZeroMQ may be found
#
# Azmq_FOUND - True of ZeroMQ found
# Azmq_INCLUDE_DIRS - Location of ZeroMQ includes
# Azmq_LIBRARIS - ZeroMQ libraries

include(FindPackageHandleStandardArgs)

if ("$ENV{AZMQ_ROOT}" STREQUAL "")
  find_path(_Azmq_ROOT NAMES azmq/version.hpp HINTS ${AZMQ_ROOT}/include)
else()
  set(_Azmq_ROOT "$ENV{AZMQ_ROOT}")
endif()

find_path(Azmq_INCLUDE_DIRS NAMES azmq/version.hpp HINTS ${_Azmq_ROOT})
set(_AzmqVersion_HPP ${Azmq_INCLUDE_DIRS}/azmq/version.hpp)

function(_azmqver_EXTRACT _Azmq_VER_COMPONENT _Azmq_VER_OUTPUT)
  execute_process(
        COMMAND grep "#define ${_Azmq_VER_COMPONENT}"
        COMMAND cut -d\  -f3
        RESULT_VARIABLE _azmqver_RESULT
        OUTPUT_VARIABLE _azmqver_OUTPUT
        INPUT_FILE ${_AzmqVersion_HPP}
        OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(${_Azmq_VER_OUTPUT} ${_azmqver_OUTPUT} PARENT_SCOPE)     
endfunction()

_azmqver_EXTRACT("AZMQ_VERSION_MAJOR" Azmq_VERSION_MAJOR)
_azmqver_EXTRACT("AZMQ_VERSION_MINOR" Azmq_VERSION_MINOR)

set(Azmq_FIND_VERSION_EXACT "${Azmq_VERSION_MAJOR}.${Azmq_VERSION_MINOR}")
find_package_handle_standard_args(Azmq FOUND_VAR Azmq_FOUND
  REQUIRED_VARS Azmq_INCLUDE_DIRS
  VERSION_VAR Azmq_VERSION)

if (Azmq_FOUND)
    mark_as_advanced(Azmq_FIND_VERSION_EXACT Azmq_VERSION Azmq_INCLUDE_DIRS Azmq_LIBRARIES)
    message(STATUS "Azmq version: ${Azmq_VERSION_MAJOR}.${Azmq_VERSION_MINOR}")
endif()
