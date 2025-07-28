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
include(CheckFunctionExists)

check_function_exists(fopen HAVE_FOPEN)
if(NOT HAVE_FOPEN)
    message(FATAL_ERROR "Required function 'fopen' not found.")
endif()

check_function_exists(fclose HAVE_FCLOSE)
if(NOT HAVE_FCLOSE)
    message(FATAL_ERROR "Required function 'fclose' not found.")
endif()

check_function_exists(fread HAVE_FREAD)
if(NOT HAVE_FREAD)
    message(FATAL_ERROR "Required function 'fread' not found.")
endif()

check_function_exists(fwrite HAVE_FWRITE)
if(NOT HAVE_FWRITE)
    message(FATAL_ERROR "Required function 'fwrite' not found.")
endif()

check_function_exists(fseek HAVE_FSEEK)
if(NOT HAVE_FSEEK)
    message(FATAL_ERROR "Required function 'fseek' not found.")
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/io.mxvm
    DESTINATION include/mxvm/modules/io
    RENAME io.mxvm
)

set_target_properties(mxvm_io PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modules/io"
)

install(TARGETS mxvm_io mxvm_io_static
    LIBRARY DESTINATION lib/modules/io
    ARCHIVE DESTINATION lib/modules/io
)
