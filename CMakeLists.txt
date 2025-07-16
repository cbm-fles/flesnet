# Copyright 2013-2025 Jan de Cuveland <cmail@cuveland.de>

cmake_minimum_required(VERSION 3.16)
project(flesnet DESCRIPTION "CBM FLES Timeslice Building" VERSION 0.0 LANGUAGES CXX)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "bin")

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
git_log(GIT_COMMIT "--date=format:%Y%m%d.%H%M" "--format=git%cd.%h" "-n1")
set(PROJECT_VERSION_GIT ${PROJECT_VERSION}~${GIT_COMMIT})
configure_file("${PROJECT_SOURCE_DIR}/config/GitRevision.cpp.in"
               "${CMAKE_BINARY_DIR}/config/GitRevision.cpp" @ONLY)

if(DEFINED ENV{SIMPATH})
  message(WARNING "SIMPATH set, using Fairroot external packages in build.")
  set(SIMPATH $ENV{SIMPATH})
  list(PREPEND CMAKE_PREFIX_PATH ${SIMPATH})
  set(Boost_NO_SYSTEM_PATHS TRUE)
  set(Boost_ROOT $ENV{SIMPATH})
endif()

set(INCLUDE_ZMQ TRUE CACHE BOOL "Download, build and statically link ZeroMQ library.")

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

  # override ZMQ's cmake version allowing builds with modern cmakes
  set(__cmake_params
    "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.16"
    "-DCMAKE_SUPPRESS_DEVELOPER_WARNINGS:BOOL=TRUE"
    "-DWITH_LIBBSD:BOOL=FALSE"
    "-DWITH_LIBSODIUM:BOOL=FALSE"
  )

  if(APPLE AND CMAKE_OSX_SYSROOT)
    set(__cmake_params ${__cmake_params}
      "-DCMAKE_OSX_SYSROOT:PATH=${CMAKE_OSX_SYSROOT}"
    )
  endif()

  ExternalProject_Add(
    zeromq
    GIT_REPOSITORY "https://github.com/zeromq/libzmq.git"
    GIT_TAG "622fc6dde99ee172ebaa9c8628d85a7a1995a21d" # v4.3.5
    GIT_CONFIG advice.detachedHead=false
    CMAKE_CACHE_ARGS ${__cmake_params}
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
  if(CMAKE_VERSION VERSION_GREATER "3.26.0")
    set(FetchContent_Declare_SYSTEM "SYSTEM")
  else()
    set(FetchContent_Declare_SYSTEM "")
  endif()
  message(STATUS "Downloading and building fmtlib.")
  include(FetchContent)
  FetchContent_Declare(
    fmtlib
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0
    ${FetchContent_Declare_SYSTEM})
  FetchContent_MakeAvailable(fmtlib) # Adds fmt::fmt
endif()

if(APPLE)
  set(Boost_USE_MULTITHREADED TRUE)
endif()
find_package(Boost 1.74.0 REQUIRED CONFIG COMPONENTS filesystem log program_options serialization system thread unit_test_framework iostreams)
find_package(Threads REQUIRED)

find_package(LIBFABRIC)
find_package(RDMA)
find_package(PDA)
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
    NAMES "clang-tidy-15" "clang-tidy-14" "clang-tidy"
    DOC "Path to clang-tidy executable"
  )
  if(NOT CLANG_TIDY)
    message(STATUS "Binary not found: clang-tidy. Skipping checks.")
  else()
    message(STATUS "Found clang-tidy: ${CLANG_TIDY}")
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)

if( NOT "arm64" STREQUAL ${CMAKE_SYSTEM_PROCESSOR} )
    add_compile_options(-msse4.2)
endif()

add_compile_options(-Wall -Wpedantic -Wextra -Winit-self -Wundef -Wold-style-cast -Woverloaded-virtual -Wwrite-strings -Wnon-virtual-dtor -fno-omit-frame-pointer)

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

set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
     OWNER_READ OWNER_WRITE OWNER_EXECUTE
     GROUP_READ GROUP_EXECUTE
     WORLD_READ WORLD_EXECUTE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

add_subdirectory(app/mstool)
add_subdirectory(app/archive_validator)
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
  set(CPACK_GENERATOR DEB)
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_GIT})
  set(CPACK_PACKAGE_CONTACT "Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "")
  set(CPACK_COMPONENT_BIN_DESCRIPTION "CBM FLES timeslice building
The First-Level Event Selector (FLES) system of the CBM experiment employs a
scheme of timeslices (consisting of microslices) instead of events in data
aquisition. This software aims to implement the timeslice building process
between several nodes of the FLES cluster over an Infiniband network.
.
This package includes the flesnet executables.")
  set(CPACK_COMPONENT_FLESCTL_DESCRIPTION "CBM FLES timeslice building control system
The First-Level Event Selector (FLES) system of the CBM experiment employs a
scheme of timeslices (consisting of microslices) instead of events in data
aquisition. This software aims to implement the timeslice building process
between several nodes of the FLES cluster over an Infiniband network.
.
This package includes the flesctl system used in the mFLES installation.
WARNING: This package is experimental and not intended for general use.")
  set(CPACK_DEBIAN_PACKAGE_SECTION misc)
  set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/cbm-fles/flesnet")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_STRIP_FILES TRUE)
  INCLUDE(CPack)
endif()
