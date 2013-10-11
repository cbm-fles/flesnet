cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(flesnet)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(lib/fles_core)
#add_subdirectory(lib/fles_ipc)

add_subdirectory(app/flesnet)
add_subdirectory(app/ipc-demo)

add_subdirectory(doc)

add_subdirectory(contrib)
