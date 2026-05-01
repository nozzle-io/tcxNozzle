cmake_minimum_required(VERSION 3.16)
project(tcxNozzle-deps)

set(NOZZLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/nozzle)
