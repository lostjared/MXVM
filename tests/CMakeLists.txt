cmake_minimum_required(VERSION 3.16)
project(mxvm-test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(mxvm-test codetest.cpp)
target_include_directories(mxvmc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/mxvm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/include
)
include(GNUInstallDirs)
set_target_properties(mxvm-test PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
)
install(TARGETS mxvm-test
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)