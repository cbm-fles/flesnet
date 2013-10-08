cmake_minimum_required(VERSION 2.8)
include(cmake/PCHSupport.cmake)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

project(flesnet)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -pedantic -Wold-style-cast -Wfloat-equal -Woverloaded-virtual -Wundef -O3")

find_package(Boost 1.48 COMPONENTS program_options serialization REQUIRED)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

add_custom_command(
  OUTPUT flesnet.cfg
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/contrib/flesnet.cfg flesnet.cfg
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/flesnet.cfg)

add_custom_command(
  OUTPUT run
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/contrib/run run
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/run)

add_custom_command(
  OUTPUT verbs.supp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/contrib/verbs.supp verbs.supp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/verbs.supp)

add_executable(flesnet Parameters.cpp IBConnection.cpp FlibPatternGenerator.cpp InputChannelSender.cpp InputChannelConnection.cpp ComputeNodeConnection.cpp ComputeBuffer.cpp InputNodeApplication.cpp ComputeNodeApplication.cpp ThreadContainer.cpp main.cpp flesnet.cfg run verbs.supp)
add_precompiled_header(flesnet ${CMAKE_SOURCE_DIR}/include.hpp)

target_link_libraries(flesnet ibverbs)
target_link_libraries(flesnet rdmacm)
target_link_libraries(flesnet rt)
target_link_libraries(flesnet numa)
target_link_libraries(flesnet ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_executable(pattern-check pattern-check.cpp)
target_link_libraries(pattern-check rt)

add_executable(raw-storage raw-storage.cpp)
target_link_libraries(raw-storage rt)
target_link_libraries(raw-storage ${Boost_SERIALIZATION_LIBRARY})

# Documentation
include(FindDoxygen)
if(DOXYGEN_FOUND)
    add_subdirectory(doc)
else(DOXYGEN_FOUND)
    message(STATUS "WARNING: Failed to find Doxygen - won't build documentation")
endif(DOXYGEN_FOUND)
