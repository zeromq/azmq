include(CMakeFindDependencyMacro)
find_dependency(Boost 1.68 OPTIONAL_COMPONENTS system)

include(${CMAKE_CURRENT_LIST_DIR}/FindAzmqLibzmq.cmake)
find_dependency(Threads)
include("${CMAKE_CURRENT_LIST_DIR}/azmqTargets.cmake")
