# Copyright 2013, 2015, 2016 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

# The shared memory producer classes need std::format. Projects building
# fles_ipc with an older standard library (e.g., GCC 12) get it without them.
include(CheckCXXSourceCompiles)
set(OLD_CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD 23)
check_cxx_source_compiles("
#include <format>
int main() { return static_cast<int>(std::format(\"{}\", 1).size()); }"
  FLES_IPC_HAS_STD_FORMAT)
set(CMAKE_CXX_STANDARD ${OLD_CMAKE_CXX_STANDARD})
if(NOT FLES_IPC_HAS_STD_FORMAT)
  message(STATUS "=> No std::format, building fles_ipc without TimesliceShmBuffer and TimesliceShmSink.")
  list(REMOVE_ITEM LIB_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/TimesliceShmBuffer.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/TimesliceShmSink.cpp)
endif()

#------------------------------------------------------------------------------#
set(OLD_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(OLD_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})

#set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} BOOST_ALL_DYN_LINK)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${Boost_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${Boost_IOSTREAMS_LIBRARY})

# Check if the Boost iostream was compiled with ZSTD (un)compression support
CHECK_CXX_SOURCE_COMPILES("
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/filtering_stream.hpp>

int main(int argc, char *argv[])
{
  std::unique_ptr<boost::iostreams::filtering_istream> in_;
  in_ = std::make_unique<boost::iostreams::filtering_istream>();
  in_->push(boost::iostreams::zstd_decompressor());

  return 0;
}" BOOST_IOS_HAS_ZSTD)

if(BOOST_IOS_HAS_ZSTD)
  message(STATUS "=> Boost::iostream with ZSTD filter found.")
else()
  message(STATUS "=> Boost::iostream does not support ZSTD filter.")
endif()

set(CMAKE_REQUIRED_DEFINITIONS ${OLD_CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_INCLUDES ${OLD_CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})
#------------------------------------------------------------------------------#

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})

# The headers used outside flesnet stay C++17; SubTimeslice.hpp,
# OptionValues.hpp and TimesliceShm*.hpp need C++23
target_compile_features(fles_ipc PUBLIC cxx_std_17 PRIVATE cxx_std_23)

target_compile_definitions(fles_ipc PUBLIC BOOST_ALL_DYN_LINK)
if(BOOST_IOS_HAS_ZSTD)
  target_compile_definitions(fles_ipc PUBLIC BOOST_IOS_HAS_ZSTD)
endif()

target_include_directories(fles_ipc PUBLIC .)

target_include_directories(fles_ipc SYSTEM
  PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(fles_ipc
  PUBLIC shm_ipc
  PUBLIC zmq::cppzmq
  PUBLIC logging
  PUBLIC ${Boost_FILESYSTEM_LIBRARY}
  PUBLIC ${Boost_REGEX_LIBRARY}
  PUBLIC ${Boost_SERIALIZATION_LIBRARY}
  PUBLIC ${Boost_IOSTREAMS_LIBRARY}
)

install(TARGETS fles_ipc DESTINATION lib)
install(FILES ${LIB_HEADERS} DESTINATION include)

add_library(fles::fles_ipc ALIAS fles_ipc)
