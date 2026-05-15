cmake_minimum_required(VERSION 3.13)
project(BBPass)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)

add_library(MemoryAnalysisPass MODULE MemoryAnalysisPass.cpp)