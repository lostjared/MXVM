cmake_minimum_required(VERSION 3.10)
project(mxvm_io)
set(SOURCES io.cpp)
add_library(mxvm_io SHARED ${SOURCES})
target_include_directories(mxvm_io
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mxvm_io
    PUBLIC
        mxvm
)
add_library(mxvm_io_static STATIC io.c)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/io.mxvm
    DESTINATION include/mxvm/modules/io
    RENAME io.mxvm
)

set_target_properties(mxvm_io PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../../lib/modules/io"
)

install(TARGETS mxvm_io mxvm_io_static
    LIBRARY DESTINATION lib/modules/io
    ARCHIVE DESTINATION lib/modules/io
)
