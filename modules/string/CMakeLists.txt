cmake_minimum_required(VERSION 3.10)
project(mxvm_string)
set(SOURCES string.cpp)
add_library(mxvm_string SHARED ${SOURCES})
target_include_directories(mxvm_string
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mxvm_string
    PUBLIC
        mxvm
)
add_library(mxvm_string_static STATIC string.c)

set_target_properties(mxvm_string PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/modules/string"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/string.mxvm
    DESTINATION include/mxvm/modules/string
    RENAME string.mxvm
)

install(TARGETS mxvm_string mxvm_string_static
    LIBRARY DESTINATION lib/modules/string
    ARCHIVE DESTINATION lib/modules/string
)