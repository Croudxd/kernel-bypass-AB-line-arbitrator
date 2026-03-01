cmake_minimum_required(VERSION 3.14)
project(kernel-bypass-AB-line-arbitrator C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

include(FetchContent)
find_package(Threads REQUIRED)

add_subdirectory(common)
add_subdirectory(xdp)
add_subdirectory(feeder)
add_subdirectory(userspace)
