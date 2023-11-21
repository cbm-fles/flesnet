# Copyright 2013-2021 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 3.14)
project(flesnet DESCRIPTION "CBM FLES Timeslice Building")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# emit 'compile_commands.json' used by some IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  list(PREPEND CMAKE_PREFIX_PATH ${SIMPATH})
  set(Boost_NO_SYSTEM_PATHS TRUE)
  set(Boost_ROOT $ENV{SIMPATH})
endif()

set(INCLUDE_ZMQ FALSE CACHE BOOL "Download, build and statically link ZeroMQ library.")

include(ExternalProject)
set_directory_properties(PROPERTIES EP_UPDATE_DISCONNECTED true)
set_directory_properties(PROPERTIES EP_PREFIX ${PROJECT_BINARY_DIR})

# find or fetch ZeroMQ
if(INCLUDE_ZMQ)
  message(STATUS "Downloading and building static ZeroMQ library.")

  if ("${CMAKE_GENERATOR}" MATCHES "Make")
    set(ZMQ_BUILD_COMMAND "$(MAKE)")
  elseif("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(ZMQ_BUILD_COMMAND "ninja")
  else()
    set(ZMQ_BUILD_COMMAND "${CMAKE_COMMAND} --build . --target")
  endif()

  ExternalProject_Add(
    zeromq
    GIT_REPOSITORY "https://github.com/zeromq/libzmq.git"
    GIT_TAG "4097855ddaaa65ed7b5e8cb86d143842a594eebd" # v4.3.4
    GIT_CONFIG advice.detachedHead=false
    PATCH_COMMAND patch --forward -p1 -i "${CMAKE_SOURCE_DIR}/fix_gnutls_macosx_v4.3.4.patch" || echo "Patch was probably already applied..." # '|| ...': allow complete recompile (cmake --build build --clean-first) without failing due to the patch has been already applied
    CMAKE_CACHE_ARGS
    -DCMAKE_SUPPRESS_DEVELOPER_WARNINGS:BOOL=TRUE
    -DWITH_LIBBSD:BOOL=FALSE
    -DWITH_LIBSODIUM:BOOL=FALSE
    BUILD_COMMAND ${ZMQ_BUILD_COMMAND} libzmq-static
    BUILD_BYPRODUCTS src/zeromq-build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}zmq${CMAKE_STATIC_LIBRARY_SUFFIX}
    INSTALL_COMMAND ""
  )

  # pre-create empty directory to make INTERFACE_INCLUDE_DIRECTORIES happy
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/src/zeromq/include)

  add_library(zmq::libzmq STATIC IMPORTED GLOBAL)
  add_dependencies(zmq::libzmq zeromq)

  find_package("GnuTLS")
  if(GNUTLS_FOUND)
    target_link_libraries(zmq::libzmq
                          INTERFACE ${GNUTLS_LIBRARIES}
    )
  endif()

  set_target_properties(zmq::libzmq PROPERTIES
    IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/src/zeromq-build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}zmq${CMAKE_STATIC_LIBRARY_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_BINARY_DIR}/src/zeromq/include
    INTERFACE_COMPILE_DEFINITIONS ZMQ_BUILD_DRAFT_API=1
  )
elseif(DEFINED SIMPATH)
  find_package(ExternalZMQ REQUIRED)
  add_library(zmq::libzmq IMPORTED GLOBAL)
  set_target_properties(zmq::libzmq PROPERTIES
    IMPORTED_LOCATION ${ZMQ_LIBRARY_SHARED}
    INTERFACE_INCLUDE_DIRECTORIES ${ZMQ_INCLUDE_DIRS}
    INTERFACE_COMPILE_DEFINITIONS ZMQ_BUILD_DRAFT_API=1
  )
elseif(APPLE)
  find_package(cppzmq REQUIRED)
  set(ZMQ_INCLUDE_DIRS ${PC_LIBZMQ_INCLUDE_DIRS} /usr/local/include)
  add_library(zmq::libzmq IMPORTED GLOBAL)
  set_target_properties(zmq::libzmq PROPERTIES
    IMPORTED_LOCATION ${ZeroMQ_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${ZMQ_INCLUDE_DIRS}
    INTERFACE_COMPILE_DEFINITIONS ZMQ_BUILD_DRAFT_API=1
  )
else()
  find_package(PkgConfig)
  pkg_search_module(zeromq REQUIRED IMPORTED_TARGET GLOBAL libzmq)
  add_library(zmq::libzmq ALIAS PkgConfig::zeromq)
endif()

# find or fetch CppZmq
find_file(CPPZMQ zmq.hpp)
find_file(CPPZMQ_ADDON zmq_addon.hpp)
if(INCLUDE_ZMQ OR CPPZMQ STREQUAL "CPPZMQ-NOTFOUND" OR CPPZMQ_ADDON STREQUAL "CPPZMQ_ADDON-NOTFOUND")
  message(STATUS "Downloading CppZmq header-only library.")
  ExternalProject_Add(
    cppzmq
    GIT_REPOSITORY "https://github.com/zeromq/cppzmq.git"
    GIT_TAG "dd663fafd830466d34cba278c2cfd0f92eb67614" # v4.8.1
    GIT_CONFIG advice.detachedHead=false
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )

  add_library(zmq::cppzmq INTERFACE IMPORTED GLOBAL)
  add_dependencies(zmq::cppzmq cppzmq)
  set_target_properties(zmq::cppzmq PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_BINARY_DIR}/src/cppzmq
    INTERFACE_LINK_LIBRARIES zmq::libzmq
  )
else()
  add_library(zmq::cppzmq ALIAS PkgConfig::zeromq)
endif()

# fetch libfmt formatting library, used by lib/monitoring. Should be already included in FairSoft.
find_package(fmt CONFIG)
if(NOT fmt_FOUND)
  message(STATUS "Downloading and building fmtlib.")
  include(FetchContent)
  FetchContent_Declare(
    fmtlib
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 8.1.1)
  FetchContent_MakeAvailable(fmtlib) # Adds fmt::fmt
endif()

if(APPLE)
  set(Boost_USE_MULTITHREADED TRUE)
endif()
find_package(Boost 1.65.0 REQUIRED COMPONENTS filesystem log program_options serialization system thread unit_test_framework regex iostreams)
find_package(Threads REQUIRED)

find_package(LIBFABRIC)
find_package(RDMA)
find_package(PDA 11.6.7 EXACT)
find_package(NUMA)
find_package(Doxygen)

find_package(OpenSSL REQUIRED)
if(APPLE)
  find_package(ZSTD)
  get_filename_component(ZSTD_LIB_DIR ${ZSTD_LIBRARY} DIRECTORY)
endif()

set(USE_RDMA TRUE CACHE BOOL "Use RDMA libraries and build RDMA transport.")
if(USE_RDMA AND NOT RDMA_FOUND)
  message(STATUS "Libraries not found: rdma. Building without RDMA transport.")
endif()

set(USE_LIBFABRIC TRUE CACHE BOOL "Use LIBFABRIC libraries and build LIBFABRIC transport.")
if(USE_LIBFABRIC AND NOT LIBFABRIC_FOUND)
	message(STATUS "Library not found: libfabric. Building without.")
endif()

set(USE_PDA TRUE CACHE BOOL "Use libpda and build CRI interface.")
if(USE_PDA AND NOT PDA_FOUND)
  message(STATUS "Library not found: libpda. Building without CRI interface.")
endif()

set(USE_NUMA TRUE CACHE BOOL "Use libnuma to schedule on specific NUMA nodes.")
if(USE_NUMA AND NOT NUMA_FOUND)
  message(STATUS "Library not found: libnuma. Building without.")
endif()

set(USE_DOXYGEN TRUE CACHE BOOL "Generate documentation using doxygen.")
if(USE_DOXYGEN AND NOT DOXYGEN_FOUND)
	message(STATUS "Binary not found: Doxygen. Not building documentation.")
endif()

set(USE_CLANG_TIDY FALSE CACHE BOOL "Run clang-tidy during build.")
if(USE_CLANG_TIDY)
  find_program(CLANG_TIDY
    NAMES "clang-tidy-12" "clang-tidy-11" "clang-tidy"
    DOC "Path to clang-tidy executable"
  )
  if(NOT CLANG_TIDY)
    message(STATUS "Binary not found: clang-tidy. Skipping checks.")
  else()
    message(STATUS "Found clang-tidy: ${CLANG_TIDY}")
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-msse4.2 -Wall -Wpedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor -fno-omit-frame-pointer)

if (COVERALLS)
  include(Coveralls)
  coveralls_turn_on_coverage()
endif()

add_subdirectory(lib/logging)
add_subdirectory(lib/monitoring)
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
if (USE_PDA AND PDA_FOUND)
  add_subdirectory(lib/cri)
  add_subdirectory(lib/pda)
endif()
unset(CMAKE_CXX_CLANG_TIDY)
if (USE_LIBFABRIC AND LIBFABRIC_FOUND)
  add_subdirectory(lib/fles_libfabric)
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


add_subdirectory(app/mstool)
add_subdirectory(app/msconsumer)
add_subdirectory(app/tsclient)
add_subdirectory(app/flesnet)
if (USE_PDA AND PDA_FOUND)
  add_subdirectory(app/cri_tools)
  add_subdirectory(app/cri_cfg)
  add_subdirectory(app/cri_server)
endif()
unset(CMAKE_CXX_CLANG_TIDY)
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

if (UNIX)
  set (CPACK_GENERATOR "DEB")
  set (CPACK_PACKAGE_CONTACT "Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>")
  set (CPACK_DEBIAN_PACKAGE_SECTION "misc")
  set (CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/cbm-fles/flesnet")
  set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set (CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  INCLUDE(CPack)
endif()
