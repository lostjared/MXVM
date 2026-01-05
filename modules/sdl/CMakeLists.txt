cmake_minimum_required(VERSION 3.10)
project(mxvm_sdl)
set(SOURCES sdl.cpp)
add_library(mxvm_sdl SHARED ${SOURCES})
add_library(mxvm_sdl_static STATIC sdl.c)
include(CheckFunctionExists)
find_package(SDL2 REQUIRED)
if(NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 library not found. Please install SDL2.")
endif()
find_package(SDL2_ttf REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")


target_include_directories(mxvm_sdl
    PRIVATE ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mxvm_sdl
    PRIVATE ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    mxvm
    mxvm_sdl_static

)
target_include_directories(mxvm_sdl_static
    PRIVATE ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mxvm_sdl_static
    PRIVATE ${SDL2_LIBRARIES}
    SDL2_ttf
    mxvm
)

set_target_properties(mxvm_sdl PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modules/sdl"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sdl.mxvm
    DESTINATION include/mxvm/modules/sdl
    RENAME sdl.mxvm
)

install(TARGETS mxvm_sdl mxvm_sdl_static
    LIBRARY DESTINATION lib/modules/sdl
    ARCHIVE DESTINATION lib/modules/sdl
)

if(WIN32)
    install(TARGETS mxvm_sdl
        RUNTIME DESTINATION lib/modules/sdl
    )
endif()
