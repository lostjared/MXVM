cmake_minimum_required(VERSION 3.16)
project(vm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vm main.cpp)

target_include_directories(vm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/mxvm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/include
)

target_link_libraries(vm PUBLIC mxvm)