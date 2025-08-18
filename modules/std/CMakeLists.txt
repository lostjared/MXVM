cmake_minimum_required(VERSION 3.10)
project(mxvm_std)
set(SOURCES std.cpp)
add_library(mxvm_std SHARED ${SOURCES})
add_library(mxvm_std_static STATIC std.c)
include(CheckFunctionExists)

target_include_directories(mxvm_std
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mxvm_std
    PRIVATE 
    mxvm
    mxvm_std_static
)
target_include_directories(mxvm_std_static
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mxvm_std_static
    PRIVATE 
    mxvm
)

set_target_properties(mxvm_sdl PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modules/sdl"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sdl.mxvm
    DESTINATION include/mxvm/modules/std
    RENAME std.mxvm
)

install(TARGETS mxvm_sdl mxvm_std_static
    LIBRARY DESTINATION lib/modules/sdl
    ARCHIVE DESTINATION lib/modules/sdl
)

if(WIN32)
    install(TARGETS mxvm_sdl
        RUNTIME DESTINATION lib/modules/sdl
    )
endif()
