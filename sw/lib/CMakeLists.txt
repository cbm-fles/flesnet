cmake_minimum_required(VERSION 2.8)

project(libflib)

set(HW_DIR ${PROJECT_SOURCE_DIR}/../../hw)
set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -Wno-variadic-macros -pedantic -O3")

set(LIB_SRC
 ${PROJECT_SOURCE_DIR}/src/rorcfs_bar.cpp
 ${PROJECT_SOURCE_DIR}/src/rorcfs_buffer.cpp
 ${PROJECT_SOURCE_DIR}/src/rorcfs_device.cpp
 ${PROJECT_SOURCE_DIR}/src/rorcfs_dma_channel.cpp
 ${PROJECT_SOURCE_DIR}/src/rorcfs_dma_monitor.cpp
 )
set(LIB_HDR
 ${PROJECT_SOURCE_DIR}/include/flib.h
 ${PROJECT_SOURCE_DIR}/include/flib/rorcfs_bar.hh
 ${PROJECT_SOURCE_DIR}/include/flib/rorcfs_buffer.hh
 ${PROJECT_SOURCE_DIR}/include/flib/rorcfs_device.hh
 ${PROJECT_SOURCE_DIR}/include/flib/rorcfs_dma_channel.hh
 ${PROJECT_SOURCE_DIR}/include/flib/rorcfs_dma_monitor.hh
 ${PROJECT_SOURCE_DIR}/include/flib/rorcfs.h
)

add_library(flib SHARED ${LIB_HDR} ${LIB_SRC})
add_library(flib-static STATIC ${LIB_HDR} ${LIB_SRC})
set_target_properties(flib-static PROPERTIES OUTPUT_NAME flib)

add_custom_target(hw_header COMMAND make cp_header WORKING_DIRECTORY ${HW_DIR} COMMENT "Building headers")
add_dependencies(flib hw_header)
add_dependencies(flib-static hw_header)
                            
install(TARGETS flib flib-static
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
