
cmake_minimum_required(VERSION 3.16)
project(scanner LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(SCANNER_SRC
    scanner.cpp
    string_buffer.cpp
    types.cpp
)
add_library(scanner STATIC ${SCANNER_SRC})
target_include_directories(scanner PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(scanner PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(scanner PUBLIC cxx_std_20)