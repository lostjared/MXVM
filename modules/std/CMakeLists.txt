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

set_target_properties(mxvm_std PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modules/std"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/std.mxvm
    DESTINATION include/mxvm/modules/std
    RENAME std.mxvm
)

install(TARGETS mxvm_std mxvm_std_static
    LIBRARY DESTINATION lib/modules/std
    ARCHIVE DESTINATION lib/modules/std
)

if(WIN32)
    install(TARGETS mxvm_std
        RUNTIME DESTINATION lib/modules/std
    )
endif()
    