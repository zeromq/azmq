include(CMakeFindDependencyMacro)
find_dependency(Boost 1.68 CONFIG COMPONENTS headers)

include(${CMAKE_CURRENT_LIST_DIR}/FindAzmqLibzmq.cmake)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_dependency(Threads)
include("${CMAKE_CURRENT_LIST_DIR}/azmqTargets.cmake")
