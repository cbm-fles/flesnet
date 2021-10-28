# Copyright 2013-2021 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 3.14)
project(flesnet DESCRIPTION "CBM FLES Timeslice Building")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(COVERALLS "Turn on coveralls support" OFF)
option(COVERALLS_UPLOAD "Upload the generated coveralls json" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(GitSubmodule)

include(GetGitRevisionDescription)
git_describe(GIT_REVISION "--all" "--tags" "--long" "--always")
configure_file("${PROJECT_SOURCE_DIR}/config/GitRevision.cpp.in"
               "${CMAKE_BINARY_DIR}/config/GitRevision.cpp" @ONLY)

if(DEFINED ENV{SIMPATH})
  message(WARNING "SIMPATH set, using Fairroot external packages in build.")
  set(SIMPATH $ENV{SIMPATH})
  set(Boost_NO_SYSTEM_PATHS TRUE)
  set(BOOST_ROOT $ENV{SIMPATH})
endif()

set(INCLUDE_ZMQ TRUE CACHE BOOL "Download, build and statically link ZeroMQ library.")
if(INCLUDE_ZMQ)
  message(STATUS "Downloading and building static ZeroMQ library.")
  include(ExternalProject)
  set_directory_properties(PROPERTIES EP_UPDATE_DISCONNECTED true)
  set_directory_properties(PROPERTIES EP_PREFIX ${PROJECT_BINARY_DIR})
  ExternalProject_Add(
    zeromq
    GIT_REPOSITORY "https://github.com/zeromq/libzmq.git"
    GIT_TAG "4097855ddaaa65ed7b5e8cb86d143842a594eebd" # v4.3.4
    GIT_CONFIG advice.detachedHead=false
    CMAKE_CACHE_ARGS -DCMAKE_SUPPRESS_DEVELOPER_WARNINGS:BOOL=TRUE
    BUILD_COMMAND $(MAKE) libzmq-static
    INSTALL_COMMAND ""
  )
  ExternalProject_Add(
    cppzmq
    GIT_REPOSITORY "https://github.com/zeromq/cppzmq.git"
    GIT_TAG "dd663fafd830466d34cba278c2cfd0f92eb67614" # v4.8.1
    GIT_CONFIG advice.detachedHead=false
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )

  # CMake hack: pre-create empty directory to make INTERFACE_INCLUDE_DIRECTORIES work
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/src/zeromq/include)

  add_library(libzmq-static STATIC IMPORTED GLOBAL)
  add_dependencies(libzmq-static zeromq)
  target_link_libraries(libzmq-static
    INTERFACE gnutls
    INTERFACE bsd
    INTERFACE sodium
  )
  set_target_properties(libzmq-static PROPERTIES
    IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/src/zeromq-build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}zmq${CMAKE_STATIC_LIBRARY_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_BINARY_DIR}/src/zeromq/include
    INTERFACE_COMPILE_DEFINITIONS ZMQ_BUILD_DRAFT_API=1
  )

  add_library(cppzmq-static INTERFACE IMPORTED GLOBAL)
  add_dependencies(cppzmq-static cppzmq)
  set_target_properties(cppzmq-static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_BINARY_DIR}/src/cppzmq
    INTERFACE_LINK_LIBRARIES libzmq-static
  )
elseif(DEFINED SIMPATH)
  find_package(ExternalZMQ REQUIRED)
  add_library(cppzmq-static INTERFACE IMPORTED GLOBAL)
  # ...
else()
  find_package(PkgConfig)
  #find_library(ZMQ_LIBRARIES zmq)
  pkg_search_module(ZMQ REQUIRED libzmq)
  # ...
endif()

if(APPLE)
  find_package(cppzmq REQUIRED)
  set(ZMQ_INCLUDE_DIRS ${PC_LIBZMQ_INCLUDE_DIRS} /usr/local/include)
  set(ZMQ_LIBRARIES ${ZeroMQ_LIBRARY})
  set(Boost_USE_MULTITHREADED TRUE)
endif()

find_package(Boost 1.65.0 REQUIRED COMPONENTS filesystem log program_options serialization system thread unit_test_framework regex iostreams)
find_package(Threads REQUIRED)

find_package(LIBFABRIC)
find_package(RDMA)
find_package(PDA 11.5.7 EXACT)
find_package(CPPREST)
find_package(NUMA)
find_package(Doxygen)

set(USE_RDMA TRUE CACHE BOOL "Use RDMA libraries and build RDMA transport.")
if(USE_RDMA AND NOT RDMA_FOUND)
  message(STATUS "Libraries not found: rdma. Building without RDMA transport.")
endif()

set(USE_LIBFABRIC TRUE CACHE BOOL "Use LIBFABRIC libraries and build LIBFABRIC transport.")
if(USE_LIBFABRIC AND NOT LIBFABRIC_FOUND)
	message(STATUS "Library not found: libfabric. Building without.")
endif()

set(USE_PDA TRUE CACHE BOOL "Use libpda and build FLIB/CRI interface.")
if(USE_PDA AND NOT PDA_FOUND)
  message(STATUS "Library not found: libpda. Building without FLIB/CRI interface.")
endif()

set(USE_NUMA TRUE CACHE BOOL "Use libnuma to schedule on specific NUMA nodes.")
if(USE_NUMA AND NOT NUMA_FOUND)
  message(STATUS "Library not found: libnuma. Building without.")
endif()

set(USE_DOXYGEN TRUE CACHE BOOL "Generate documentation using doxygen.")
if(USE_DOXYGEN AND NOT DOXYGEN_FOUND)
	message(STATUS "Binary not found: Doxygen. Not building documentation.")
endif()

find_program(CLANG_TIDY
  NAMES "clang-tidy-12" "clang-tidy-11" "clang-tidy"
  DOC "Path to clang-tidy executable"
)
if(NOT CLANG_TIDY)
  message(STATUS "clang-tidy not found.")
else()
  message(STATUS "clang-tidy found: ${CLANG_TIDY}")
endif()

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-msse4.2 -Wall -Wpedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor -fno-omit-frame-pointer)

if (COVERALLS)
  include(Coveralls)
  coveralls_turn_on_coverage()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/etcd-cpp-api/CMakeLists.txt")
    message(FATAL_ERROR "Submodule 'etcd-cpp-api' not available! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()
add_subdirectory(external/etcd-cpp-api)

add_subdirectory(lib/logging)
add_subdirectory(lib/crcutil)
add_subdirectory(lib/flib_ipc)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()
add_subdirectory(lib/fles_ipc)
add_subdirectory(lib/fles_core)
add_subdirectory(lib/fles_zeromq)
add_subdirectory(lib/shm_ipc)
if (USE_RDMA AND RDMA_FOUND)
  add_subdirectory(lib/fles_rdma)
endif()
unset(CMAKE_CXX_CLANG_TIDY)
if (USE_LIBFABRIC AND LIBFABRIC_FOUND)
  add_subdirectory(lib/fles_libfabric)
endif()
if (USE_PDA AND PDA_FOUND)
  add_subdirectory(lib/cri)
  add_subdirectory(lib/flib)
  add_subdirectory(lib/pda)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "..." FORCE)
endif()

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()
add_subdirectory(app/tsclient)
add_subdirectory(app/mstool)
add_subdirectory(app/flesnet)
unset(CMAKE_CXX_CLANG_TIDY)
if (USE_PDA AND PDA_FOUND)
  add_subdirectory(app/cri_tools)
  add_subdirectory(app/cri_cfg)
  add_subdirectory(app/cri_server)
  add_subdirectory(app/flib_tools)
  add_subdirectory(app/flib_cfg)
  add_subdirectory(app/flib_server)
endif()
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)

add_subdirectory(contrib)
add_subdirectory(contrib/flesctl)
if (USE_DOXYGEN AND DOXYGEN_FOUND)
  add_subdirectory(doc)
endif()

if (COVERALLS)
  file(GLOB_RECURSE LIB_SRC lib/*.[ch]pp)
  coveralls_setup("${LIB_SRC}" ${COVERALLS_UPLOAD})
endif()

enable_testing()
add_subdirectory(test)

configure_file(cmake/CTestCustom.cmake ${CMAKE_BINARY_DIR})

if (UNIX)
  set (CPACK_GENERATOR "DEB")
  set (CPACK_PACKAGE_CONTACT "Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>")
  set (CPACK_DEBIAN_PACKAGE_SECTION "misc")
  set (CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/cbm-fles/flesnet")
  set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set (CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  INCLUDE(CPack)
endif()
