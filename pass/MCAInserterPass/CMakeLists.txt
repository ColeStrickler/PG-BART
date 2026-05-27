cmake_minimum_required(VERSION 3.13)
project(BBPass)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)

add_library(MCAInserterPass MODULE MCAInserterPass.cpp)