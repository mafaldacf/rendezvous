cmake_minimum_required(VERSION 3.5.1)

project(Rendezvous)

set(CMAKE_CXX_STANDARD 14)

include(cmake/FindgRPC.cmake)
include(cmake/CompileProtos.cmake)
compile_protos(monitor)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(test)